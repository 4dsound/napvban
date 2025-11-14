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
		mSetLatencyManually.store(true);
		mManualLatency.store(latency * getNodeManager().getSamplesPerMillisecond());
		mResetReadPosition.set();
	}


	void VBANCircularBuffer::calibrateLatency()
	{
		mSetLatencyManually.store(false);
		mResetReadPosition.set();
	}


	void VBANCircularBuffer::process()
	{
		if (mResetReadPosition.check())
		{
			// The read position was requested to be reset.
			Logger::debug("VBANCircularBuffer: reset read position.");
			if (mSetLatencyManually.load())
				mLatency.store(mManualLatency.load());
			else
				mLatency.store(getBufferSize());
			mReadPosition = mWritePosition - mLatency.load();
		}
		else
		{
			// Increase the read position of the circular buffer.
			mReadPosition += getBufferSize();

			// If the read position overtakes the write position, increase the latency.
			if (mReadPosition + getBufferSize() > mWritePosition)
			{
				// This check is to avoid changing the read position when no audio is coming in.
				if (mWritePosition == mLastWritePosition)
				{
					mReadPosition = mWritePosition - mLatency.load();
					mLastWritePosition = mWritePosition;
					return;
				}
				Logger::debug("VBANCircularBuffer: Read position overtaking write position.");
				auto latency = mManualLatency.load();
				if (!mSetLatencyManually.load() && mLatency.load() < MaxLatency)
				{
					Logger::debug("Increasing latency");
					latency = mLatency.load() + LatencyDelta;
				}
				mLatency.store(latency);
				mReadPosition = mWritePosition - latency;
			}
			// If the read position is too far behind, reset the latency.
			else if (mWritePosition - mReadPosition > mLatency.load() * 2)
			{
				auto latency = mManualLatency.load();
				if (!mSetLatencyManually.load() && mLatency.load() < MaxLatency)
				{
					Logger::debug("Increasing latency");
					latency = mLatency.load() + LatencyDelta;
				}
				mLatency.store(latency);
				mReadPosition = mWritePosition - latency;
				Logger::debug("VBANCircularBuffer: Read position too far behind.");
			}
			mLastWritePosition = mWritePosition;
		}
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
