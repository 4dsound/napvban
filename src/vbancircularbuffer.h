#pragma once

#include <audio/core/audionode.h>
#include <audio/utility/audiotypes.h>
#include <audio/utility/safeptr.h>
#include <audio/utility/dirtyflag.h>

namespace nap
{

	class NAPAPI VBANCircularBuffer : public audio::Process
	{
		RTTI_ENABLE(audio::Process)

	public:
		VBANCircularBuffer(audio::NodeManager& nodeManager) : audio::Process(nodeManager) { }

		// Called from control thread
		void addStream(const std::string &name, int channelCount);
		void removeStream(const std::string &name);

		// Called from the VBAN thread
		bool write(const std::string &name, audio::DiscreteTimeValue time, const audio::MultiSampleBuffer &buffer);

		// Called from the audio threads
		void read(const std::string &name, int channel, audio::SampleBuffer &buffer);

		void setStreamChannelCount(const std::string& streamName, int channelCount);

		// Called from main thread
		void setLatency(float latency);

		float getLatency() const { return mLatency.load(); }

		int getStreamCount() const { return mStreamCount.load(); }

	private:
		void process() override;
		void sampleRateChanged(float sampleRate) override { mResetReadPosition.set(); }

		struct ProtectedBuffer
		{
			std::mutex mMutex;
			audio::MultiSampleBuffer mData;
		};
		std::map<std::string, std::unique_ptr<ProtectedBuffer>> mBuffers;

		int mSize = 4096;
		audio::DiscreteTimeValue mWritePosition = 0;
		nap::int64 mReadPosition = 0; // The read position can be negative when the write position is zeroed.
		nap::int64 mLastReadPosition = 0;
		std::atomic<float> mLatency = { 0 }; // Latency in ms
		std::mutex mMutex;
		audio::DirtyFlag mResetReadPosition;
		std::atomic<int> mStreamCount = { 0 };
	};


	class NAPAPI VBANCircularBufferReader : public audio::Node
	{
		RTTI_ENABLE(audio::Node)

	public:
		VBANCircularBufferReader(audio::NodeManager& manager) : audio::Node(manager) { }

		void init(const audio::SafePtr<VBANCircularBuffer>& circularBuffer, const std::string& streamName, int channelCount);
		void setChannelCount(int channelCount);
		int getChannelCount() const { return mOutputPins.size(); }
		audio::OutputPin& getOutputPin(int index) { return *mOutputPins[index]; }

	private:
		void process() override;

		audio::SafePtr<VBANCircularBuffer> mCircularBuffer;
		std::string mStreamName;
		std::vector<std::unique_ptr<audio::OutputPin>> mOutputPins;
	};

}
