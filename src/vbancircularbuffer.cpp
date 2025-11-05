#include "vbancircularbuffer.h"

#include "audio/core/audionodemanager.h"
#include "nap/logger.h"

namespace nap
{

	void VBANCircularBuffer::addStream(const std::string &name, int channelCount)
	{
		auto buffer = std::make_unique<ProtectedBuffer>();
		buffer->mData.resize(channelCount, mSize);

		{
			std::lock_guard<std::mutex> lock(mBufferMapMutex);
			mBufferMap[name] = std::move(buffer);
			mStreamCount++;
		}

		// Reset read and write pointers
		mWritePosition = 0;
		mReadPosition = 0;
	}


	void VBANCircularBuffer::removeStream(const std::string &name)
	{
		std::lock_guard<std::mutex> lock(mBufferMapMutex);
		mBufferMap.erase(name);
		mStreamCount--;
	}


	bool VBANCircularBuffer::write(const std::string &streamName, audio::DiscreteTimeValue time, const audio::MultiSampleBuffer &input)
	{
		std::lock_guard<std::mutex> lock(mBufferMapMutex);

		auto it = mBufferMap.find(streamName);
		if (it == mBufferMap.end())
			return false;

		auto& buffer = it->second;

		// Only write if the number of channels fits the buffer for the stream
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

		// Update the write position
		if (time > mWritePosition)
			mWritePosition = time;

		// If the time is zero it means that all the streams are restarting and the write and read positions need to be reset.
		if (time == 0 && mWritePosition != 0)
		{
			mWritePosition = 0;
			mResetReadPosition.set();
		}

		return true;
	}


	void VBANCircularBuffer::read(const std::string &streamName, int channel, audio::SampleBuffer &output)
	{
		// The read position can be negative when the stream is reset and the write position is zeroed.
		if (mReadPosition < 0)
		{
			std::fill(output.begin(), output.end(), 0.f);
			return;
		}

		auto it = mBufferMap.find(streamName);
		if (it == mBufferMap.end())
			return;

		if (it->second->mMutex.try_lock())
		{
			// Only read if the channel is within the bounds.
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
		std::lock_guard<std::mutex> lock(mBufferMapMutex);

		auto it = mBufferMap.find(streamName);
		assert(it != mBufferMap.end());

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
			// The read position was requested to be reset.
			Logger::debug("VBANCircularBuffer: reset read position.");
			mReadPosition = mWritePosition - latencyInSamples;
		}
		else
		{
			// Increase the read position of the circular buffer.
			mReadPosition += getBufferSize();

			// If the read position overtakes the write position, reset it.
			if (mReadPosition + getBufferSize() > mWritePosition)
			{
				mReadPosition = mWritePosition - latencyInSamples;

				// This check is to avoid outputting this message every buffer when no data is coming in.
				if (mReadPosition != mLastReadPosition)
					Logger::debug("VBANCircularBuffer: Read position overtaking write position. Reset read position.");
			}
			// If the read position is too far behind, reset it.
			else if (mWritePosition - mReadPosition > latencyInSamples * 2)
			{
				Logger::debug("VBANCircularBuffer: Read position too far behind. Reset read position.");
				mReadPosition = mWritePosition - latencyInSamples;
			}
		}
		mLastReadPosition = mReadPosition; // Remember previous value
	}


	void VBANCircularBufferReader::init(const audio::SafePtr<VBANCircularBuffer>& circularBuffer, const std::string &streamName, int channelCount)
	{
		mCircularBuffer = circularBuffer;
		mStreamName = streamName;

		// Create output pins
		for (int channel = 0; channel < channelCount; ++channel)
			mOutputPins.emplace_back(std::make_unique<audio::OutputPin>(this));
	}


	void VBANCircularBufferReader::setChannelCount(int channelCount)
	{
		// Recreate output pins
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
