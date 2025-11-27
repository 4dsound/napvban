#pragma once

#include <audio/core/audionode.h>
#include <audio/utility/audiotypes.h>
#include <audio/utility/safeptr.h>
#include <audio/utility/dirtyflag.h>

#include "audio/core/audionodemanager.h"

#include <vbanutils.h>

namespace nap
{
	/**
	 * An audio process that manages a circular buffer to receive the input of all incoming VBAN streams from a single sender.
	 * It takes care of synchronication and a global read position to ensure all streams are read and played back in sync.
	 */
	class NAPAPI VBANCircularBuffer : public audio::Process
	{
		RTTI_ENABLE(audio::Process)

	public:
		/**
		 * Constructor
		 * @param nodeManager The NodeManager of the system
		 */
		VBANCircularBuffer(audio::NodeManager& nodeManager);

		// Called from control thread

		/**
		 * Adds a VBAN stream to receive into the circular buffer.
		 * @param name Name of the stream
		 * @param channelCount Number of channels in the stream
		 */
		void addStream(const std::string &name, int channelCount);

		/**
		 * Removes a VBAN stream from the circular buffer.
		 * @param name Name of the stream to be removed
		 */
		void removeStream(const std::string &name);

		// Called from the VBAN receiver thread

		/**
		 *
		 * @param header
		 * @param size
		 * @return
		 */
		bool write(const VBanHeader& header, size_t size);

		// Called from the audio threads

		/**
		 * Read audio data for a certain stream from the circular buffer.
		 * Reads from the global read position of the buffer that is increased every audio callback with the current buffer size.
		 * @param name Name of the stream
		 * @param channel Channel of the stream the read.
		 * @param buffer Single channel buffer to read into. The size of the buffer will be read.
		 */
		void read(const std::string &name, int channel, audio::SampleBuffer &buffer);

		/**
		 * Sets the number of channels received for the given stream.
		 * @param streamName Name of the stream.
		 * @param channelCount Number of audio channels.
		 */
		void setStreamChannelCount(const std::string& streamName, int channelCount);

		// Called from main thread

		/**
		 * @return The latency in milliseconds, which is equal to the difference between the read and write position.
		 */
		float getLatency() const { return mLatency.load() / getNodeManager().getSamplesPerMillisecond(); }

		/**
		 * Sets the latency manually to a given amount instead of using calibration.
		 * @param latency Latency in ms
		 */
		void setLatency(float latency);

		/**
		 * Starts calibrating the latency by zeroing it and then increasing it for each bufer underrun or overflow.
		 */
		void calibrateLatency();

		/**
		 * @return The number of streamd received in the circular buffer.
		 */
		int getStreamCount() const { return mStreamCount.load(); }

		/**
		 * Acquire any error logging in a thread-safe manner.
		 * @param message
		 */
		void getErrorMessage(std::string& message) const;

	private:
		// Inherited from Process
		void process() override;
		void sampleRateChanged(float sampleRate) override { mResetReadPosition.set(); }
		void bufferSizeChanged(int bufferSize) override { mResetReadPosition.set(); }

		// Checking packet integrity
		bool checkPacket(const VBanHeader& header, size_t size);

		// Set error message
		void setError(const std::string& errorMessage);

		// Map of the (circular) buffer for each stream, protected by a mutex for resizing.
		struct ProtectedBuffer
		{
			std::mutex mMutex;
			audio::MultiSampleBuffer mData;
		};
		std::map<std::string, std::unique_ptr<ProtectedBuffer>> mBufferMap;
		std::mutex mBufferMapMutex;						// Protects the buffer map.

		int mSize = 8192;								// Size of the circular buffer in samples.
		audio::DiscreteTimeValue mWritePosition = 0;	// Current write position in the circular buffer.
		audio::DiscreteTimeValue mLastWritePosition = 0;
		audio::DiscreteTimeValue mFadeInTime = 0;
		nap::int64 mReadPosition = 0;					// The read position can be negative when the write position is zeroed.
		std::string mStreamName;						// For internal use, kept here to avoid reallocations.
		std::atomic<int> mLatency = 0;
		std::atomic<int> mManualLatency = 0;
		std::atomic<bool> mSetLatencyManually = { false };
		audio::DirtyFlag mResetReadPosition;			// This flag is set when the read position has to be recalculated from the write position.
		std::atomic<int> mStreamCount = { 0 };			// Number of streams in the circular buffer.
		static constexpr int MaxLatency = 2048;

		int mCounter = 0;

		// For error reporting
		std::string mErrorMessage;
		mutable std::mutex mErrorMessageMutex;
	};


	/**
	 * Audio node that reads audio data for onee stream from a VBANCircularBuffer.
	 */
	class NAPAPI VBANCircularBufferReader : public audio::Node
	{
		RTTI_ENABLE(audio::Node)

	public:
		VBANCircularBufferReader(audio::NodeManager& manager) : audio::Node(manager) { }

		/**
		 * Initializes the node, call after construction.
		 * @param circularBuffer Pointer to the VBANCircularBuffer it reads from.
		 * @param streamName Name of the stream it reads from.
		 * @param channelCount Number of channels this node reads and outputs.
		 *	This number has to be equal for the number of channels in the stream in order to read.
		 */
		void init(const audio::SafePtr<VBANCircularBuffer>& circularBuffer, const std::string& streamName, int channelCount);

		/**
		 * Sets number of channels this node reads and outputs.
		 * This number has to be equal for the number of channels in the stream in order to read.
		 * @param channelCount Number of channels.
		 */
		void setChannelCount(int channelCount);

		/**
		 * @return The current channel count.
		 */
		int getChannelCount() const { return mOutputPins.size(); }

		/**
		 * @return The output pin for a certain channel.
		 */
		audio::OutputPin& getOutputPin(int index) { return *mOutputPins[index]; }

	private:
		// Inherited from Node
		void process() override;

		audio::SafePtr<VBANCircularBuffer> mCircularBuffer;
		std::string mStreamName;
		std::vector<std::unique_ptr<audio::OutputPin>> mOutputPins;
	};

}
