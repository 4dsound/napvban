#include "vbancircularbuffer.h"

#include "audio/core/audionodemanager.h"
#include "nap/logger.h"

namespace nap
{

	void VBANCircularBuffer::addStream(const std::string &name, int channelCount)
	{
		std::lock_guard<std::mutex> lock(mMutex);

		auto buffer = std::make_unique<ProtectedBuffer>();
		buffer->mData.resize(channelCount, mSize);
		mBuffers[name] = std::move(buffer);

		// Reset read and write pointers
		mWritePosition = 0;
		mReadPosition = 0;

		mStreamCount++;
	}


	void VBANCircularBuffer::removeStream(const std::string &name)
	{
		std::lock_guard<std::mutex> lock(mMutex);

		mBuffers.erase(name);
		mStreamCount--;
	}


	bool VBANCircularBuffer::write(const std::string &name, audio::DiscreteTimeValue time, const audio::MultiSampleBuffer &input)
	{
		std::lock_guard<std::mutex> lock(mMutex);

		auto it = mBuffers.find(name);
		if (it == mBuffers.end())
			return false;

		auto& buffer = it->second;

		if (buffer->mData.getChannelCount() == input.getChannelCount())
		{
			auto pos = time % mSize;
			for (auto i = 0; i < input.getSize(); ++i)
			{
				for (auto channel	= 0; channel < input.getChannelCount(); ++channel)
					buffer->mData[channel][pos] = input[channel][i];
				pos++;
				if (pos >= mSize)
					pos = 0;
			}
		}

		if (time > mWritePosition)
			mWritePosition = time;

		if (time == 0 && mWritePosition != 0)
		{
			mWritePosition = 0;
			mResetReadPosition.set();
		}

		return true;
	}


	void VBANCircularBuffer::read(const std::string &name, int channel, audio::SampleBuffer &output)
	{
		if (mReadPosition < 0)
		{
			std::fill(output.begin(), output.end(), 0.f);
			return;
		}

		auto it = mBuffers.find(name);
		if (it == mBuffers.end())
			return;

		if (it->second->mMutex.try_lock())
		{
			if (channel < it->second->mData.getChannelCount())
			{
				auto& buffer = it->second->mData[channel];
				auto pos = mReadPosition % mSize;
				for (auto i = 0; i < output.size(); ++i)
				{
					output[i] = buffer[pos];
					buffer[pos] = 0.f;
					pos++;
					if (pos >= mSize)
						pos = 0;
				}
			}

			it->second->mMutex.unlock();
		}
	}


	void VBANCircularBuffer::setStreamChannelCount(const std::string &streamName, int channelCount)
	{
		std::lock_guard<std::mutex> lock(mMutex);

		auto it = mBuffers.find(streamName);
		assert(it != mBuffers.end());

		auto& buffer = it->second;
		if (buffer->mData.getChannelCount() != channelCount)
		{
			std::lock_guard<std::mutex> lock(buffer->mMutex);
			buffer->mData.resize(channelCount, mSize);
		}
	}


	void VBANCircularBuffer::setLatency(float latency)
	{
		mLatency.store(latency);
		mResetReadPosition.set();
	}


	void VBANCircularBuffer::process()
	{
		auto latencyInSamples = mLatency.load() * getNodeManager().getSamplesPerMillisecond() + getBufferSize();
		if (mResetReadPosition.check())
		{
			Logger::debug("VBANCircularBuffer: reset read position.");
			mReadPosition = mWritePosition - latencyInSamples;
		}
		else
		{
			mReadPosition += getBufferSize();
			if (mReadPosition + getBufferSize() > mWritePosition)
			{
				mReadPosition = mWritePosition - latencyInSamples;
				if (mReadPosition != mLastReadPosition)
					Logger::debug("VBANCircularBuffer: Read position overtaking write position. Reset read position.");
			}
			else if (mWritePosition - mReadPosition > latencyInSamples * 2)
			{
				Logger::debug("VBANCircularBuffer: Read position too far behind. Reset read position.");
				mReadPosition = mWritePosition - latencyInSamples;
			}
		}
		mLastReadPosition = mReadPosition;
	}


	void VBANCircularBufferReader::init(const audio::SafePtr<VBANCircularBuffer>& circularBuffer, const std::string &streamName, int channelCount)
	{
		mCircularBuffer = circularBuffer;
		mStreamName = streamName;
		for (int channel = 0; channel < channelCount; ++channel)
			mOutputPins.emplace_back(std::make_unique<audio::OutputPin>(this));
	}


	void VBANCircularBufferReader::setChannelCount(int channelCount)
	{
		mOutputPins.clear();
		for (int channel = 0; channel < channelCount; ++channel)
			mOutputPins.emplace_back(std::make_unique<audio::OutputPin>(this));
	}


	void VBANCircularBufferReader::process()
	{
		for (auto channel = 0; channel < mOutputPins.size(); ++channel)
		{
			auto& outputBuffer = getOutputBuffer(*mOutputPins[channel]);
			mCircularBuffer->read(mStreamName, channel, outputBuffer);
		}
	}

}
