#include "vbancircularbuffer.h"

#include <audio/core/audionodemanager.h>
#include <nap/logger.h>
#include <vbanutils.h>

namespace nap
{

	VBANCircularBuffer::VBANCircularBuffer(audio::NodeManager &nodeManager) : audio::Process(nodeManager)
	{
		mStreamName.reserve(VBAN_STREAM_NAME_SIZE);
	}


	bool VBANCircularBuffer::write(const VBanHeader& header, size_t size)
	{
		std::lock_guard<std::mutex> lock(mBufferMapMutex);

		// Find stream buffer
		mStreamName = header.streamname;
		auto it = mBufferMap.find(mStreamName);
		if (it == mBufferMap.end())	// Exit quietly when stream is not found
			return false;
		auto& streamBuffer = it->second;

		// Check packet integrity
		if (!checkPacket(header, size))
			return false;

		// Check supported bit depth and derive sample size
		int sample_size = 0;
		if (header.format_bit == VBAN_BITFMT_32_INT)
			sample_size = 4;
		else if (header.format_bit == VBAN_BITFMT_16_INT)
			sample_size = 2;
		else {
			setError("Unsupported bit depth.");
			return false;
		}

		// Sample rate must match the audio engine sample rate
		int const sample_rate_format = header.format_SR & VBAN_SR_MASK;
		int packet_sample_rate = 0;
		if (!utility::getSampleRateFromVBANSampleRateFormat(packet_sample_rate, sample_rate_format))
		{
			setError("Unsupported sample rate.");
			return false;
		}
		if (packet_sample_rate != getSampleRate())
		{
			setError("Sample rate mismatch.");
			return false;
		}

		const int frameCount = header.format_nbs + 1;
		const int channelCount = header.format_nbc + 1;
		const auto packetCounter = header.nuFrame;
		const audio::DiscreteTimeValue time = packetCounter * frameCount;

		// Deinterleave and convert directly into circular buffer if channel count matches
		if (streamBuffer->mData.getChannelCount() == channelCount)
		{
			auto pos = time % mSize;
			const uint8_t* data = reinterpret_cast<const uint8_t*>(&header) + VBAN_HEADER_SIZE;
			if (sample_size == 4) // 32 bit
			{
				for (int i = 0; i < frameCount; ++i)
				{
					for (int ch = 0; ch < channelCount; ++ch)
					{
						unsigned char b1 = data[0];
						unsigned char b2 = data[1];
						unsigned char b3 = data[2];
						unsigned char b4 = data[3];
						int32_t ival = (static_cast<int32_t>(b4) << 24) |
										(static_cast<int32_t>(b3) << 16) |
										(static_cast<int32_t>(b2) << 8)  |
										(0x000000ff & b1);
						float f = static_cast<float>(ival) / static_cast<float>(std::numeric_limits<int32_t>::max());
						streamBuffer->mData[ch][pos] = f;
						data += 4;
					}
					pos++;
					if (pos >= mSize) pos = 0;
				}
			}
			else // 16-bit
			{
				for (int i = 0; i < frameCount; ++i)
				{
					for (int ch = 0; ch < channelCount; ++ch)
					{
						unsigned char b1 = data[0];
						unsigned char b2 = data[1];
						int16_t ival = static_cast<int16_t>(b2) << 8 | (0x00ff & b1);
						float f = static_cast<float>(ival) / static_cast<float>(std::numeric_limits<int16_t>::max());
						streamBuffer->mData[ch][pos] = f;
						data += 2;
					}
					pos++;
					if (pos >= mSize) pos = 0;
				}
			}
		}

		// Update the write position using time derived from packet counter and frame count
		if (time > mWritePosition)
			mWritePosition = time;
		if (time == 0 && mWritePosition != 0)
		{
			mWritePosition = 0;
			mResetReadPosition.set();
		}

		// Write successful, clear error message once
		if (!mErrorMessage.empty())
		{
			std::lock_guard<std::mutex> lock(mErrorMessageMutex);
			mErrorMessage.clear();
		}

		return true;
	}


	void VBANCircularBuffer::addStream(const std::string &name, int channelCount)
	{
		auto buffer = std::make_unique<ProtectedBuffer>();
		buffer->mData.resize(channelCount, mSize);

		{
			std::lock_guard<std::mutex> lock(mBufferMapMutex);
			mBufferMap[name] = std::move(buffer);
			++mStreamCount;
		}

		// Reset read and write pointers
		mWritePosition = 0;
		mReadPosition = 0;
	}


	void VBANCircularBuffer::removeStream(const std::string &name)
	{
		std::lock_guard<std::mutex> lock(mBufferMapMutex);
		mBufferMap.erase(name);
		--mStreamCount;
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
		std::lock_guard<std::mutex> mapLock(mBufferMapMutex);

		auto it = mBufferMap.find(streamName);
		assert(it != mBufferMap.end());

		auto& buffer = it->second;
		if (buffer->mData.getChannelCount() != channelCount)
		{
			std::lock_guard<std::mutex> bufferLock(buffer->mMutex);
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


	void VBANCircularBuffer::getErrorMessage(std::string &message) const
	{
		if (mErrorMessageMutex.try_lock())
		{
			message = mErrorMessage;
			mErrorMessageMutex.unlock();
		}
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
				mLatency.store(getBufferSize() * 2.f);
			mReadPosition = mWritePosition - mLatency.load();
		}
		else
		{
			// Increase the read position of the circular buffer.
			mReadPosition += getBufferSize();

			mCounter += getBufferSize();
			if (mCounter > getSampleRate())
			{
				mCounter = 0;
				auto realLatency = mWritePosition - mReadPosition;
				Logger::debug("VBANCircularBuffer: Actual Latency: %f ms", realLatency / getNodeManager().getSamplesPerMillisecond());
			}

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
					latency = mLatency.load() + getBufferSize();
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
					latency = mLatency.load() + getBufferSize();
				}
				mLatency.store(latency);
				mReadPosition = mWritePosition - latency;
				Logger::debug("VBANCircularBuffer: Read position too far behind.");
			}
			mLastWritePosition = mWritePosition;
		}
	}


	bool VBANCircularBuffer::checkPacket(const VBanHeader& header, size_t size)
	{
		if (size > VBAN_DATA_MAX_SIZE)
		{
			setError("Packet exceeds maximum size.");
			return false;
		}

		if (header.vban != *reinterpret_cast<const int32_t*>("VBAN"))
		{
			setError("Invalid packet header ID.");
			return false;
		}

		if (header.format_nbc < 0)
		{
			setError("Channel count should be greater than zero.");
			return false;
		}

		// check protocol
		auto protocol = static_cast<VBanProtocol>(header.format_SR & VBAN_PROTOCOL_MASK);
		if (protocol != VBAN_PROTOCOL_AUDIO)
		{
			setError("Invalid protocol ID, only audio protocol supported.");
			return false;
		}

		// Check codec
		auto codec = static_cast<VBanCodec>(header.format_bit & VBAN_CODEC_MASK);
		if (codec != VBAN_CODEC_PCM)
		{
			setError("Invalid codec ID, only PCM codec supported.");
			return false;
		}

		return true;
	}


	void VBANCircularBuffer::setError(const std::string &errorMessage)
	{
		std::lock_guard<std::mutex> lock(mErrorMessageMutex);
		mErrorMessage = errorMessage;
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
