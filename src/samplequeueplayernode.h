/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// Std includes
#include <atomic>

// Nap includes
#include <audio/utility/safeptr.h>
#include <audio/utility/dirtyflag.h>
#include <utility/threading.h>

// Audio includes
#include <audio/core/audionode.h>
#include <audio/core/audionodemanager.h>

namespace nap
{
	namespace audio
	{

		/**
		 * Node that allows the queueing of samples from another thread before they are being send through the output pins.
		 * The node allows the enqueueoing of multichannel audio.
		 */
		class NAPAPI SampleQueuePlayerNode : public Node
		{
			RTTI_ENABLE(Node)

		public:
			SampleQueuePlayerNode(NodeManager& manager);

			/**
			 * Set the number of audio channels the node enqueues and outputs.
			 * Needs to be called before calling getOutputPin().
			 * @param channelCount Number of channels
			 */
			void setChannelCount(int channelCount);

			/**
			 * @return Number of channels the node enqueues and outputs.
			 */
			int getChannelCount() const { return mOutputPins.size(); }

			/**
			 * Returns output pin for a certain channel.
			 * setChannelCount() needs to be called first.
			 * @param channel Requested output channel
			 * @return OutputPin for specified channel.
			 */
			OutputPin& getOutputPin(int channel) { return *mOutputPins[channel]; }

			/**
			 * Queue any amount of samples from another thread to be played back through the output pin.
			 * @param samples Multichannel buffer with samples. The number of channels in the buffer needs to equal the number of channels of the node.
			 */
			void queueSamples(const MultiSampleBuffer& samples);

			/**
			 * Sets the maximum size of the sample queue.
			 * @param value New maximum queue size as a multiple of the audio buffersize
			 */
			void setMaxQueueSize(int value) { mMaxQueueSizeInBuffers = value; }

			/**
			 * Sets the latency that will be used to compensate for irregular supply of samples to the queue.
			 * @param numberOfBuffers Latency specified as a multiple of the audio buffersize.
			 */
			void setLatency(int numberOfBuffers);

			/**
			 * Tells the process to clear all the enqueued samples.
			 */
			void clearSpareBuffer();;

			/**
			 * @param value True if logging is enabled.
			 */
			void setVerbose(bool value) { mVerbose = value; }

		private:
			// Inherited from Node
			void process() override;
			void bufferSizeChanged(int bufferSize) override;
			void sampleRateChanged(float sampleRate) override;

			void setSpareLatency(int spareLatency); // Sets the spare latency in samples and clears the queue;
			void clearQueue();
			void fillOutputBuffers(float value);

			std::vector<std::unique_ptr<OutputPin>> mOutputPins;

			moodycamel::ConcurrentQueue<float> mQueue;  // New samples are queued here from a different thread.
			std::vector<SampleValue> mSamples; // Interleaved buffer used to read from the queue.
			std::atomic<int> mMaxQueueSizeInBuffers = { 4 }; // The amount of samples that the queue is allowed to have as multiple of the bufefrsize
			std::atomic<bool> mVerbose = { false }; // Enable logging

			int mSpareLatency = 0; // Spare latency used to compensate for irregular supply of samples.
			int mSpareLatencyInBuffers = 0; // Spare latency as a multiple of the buffersize.
			std::atomic<int> mNewSpareLatencyInBuffers = 0; // Atomic to store values for mSpareLatencyInBuffers as a multiple of the buffersize.
			bool mSavingSpare = true; // True if the Node is current;y saving samples to build a spare buffer
			DirtyFlag mClearSpareBuffer; // Flag set for the audio thread if the spare buffer has been cleared.
		};

	}
}
