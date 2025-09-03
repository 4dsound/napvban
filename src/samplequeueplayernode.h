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
		 * Node that allows the queueing of samples from another thread before they are being send through the output pin.
		 *
		 */
		class NAPAPI SampleQueuePlayerNode : public Node
		{
			RTTI_ENABLE(Node)

		public:
			SampleQueuePlayerNode(NodeManager& manager);

			/**
			 * Connect output to this pin
			 */
			OutputPin audioOutput = {this};

			/**
			 * Queue any amount of samples from another thread to be played back through the outpu pin.
			 * @param samples Pointer to floating point data of sample data
			 * @param numSamples Number of samples to be queued
			 */
			void queueSamples(const float* samples, size_t numSamples);

			/**
			 * Sets the maximum size of the sample queue.
			 * @param value New maximum queue size
			 */
			void setMaxQueueSize(int value) { mMaxQueueSize = value; }

			/**
			 * Sets the latency that will be used to compensate for irregular supply of samples to the queue.
			 * @param numberOfBuffers Latency specified as a multiple of the audio buffersize.
			 */
			void setLatency(int numberOfBuffers);

			/**
			 * Tells the process to clear the current spare buffer.
			 */
			void clearSpareBuffer() { mClearSpareBuffer.set(); };

			/**
			 * @param value True if logging is enabled.
			 */
			void setVerbose(bool value) { mVerbose = value; }

		private:
			// Inherited from Node
			void process() override;
			void bufferSizeChanged(int bufferSize) override;
			void setSpareLatency(int spareLatency); // Sets the spare latency in samples and clears the queue;

			moodycamel::ConcurrentQueue<float> mQueue;  // New samples are queued here from a different thread.
			std::vector<SampleValue> mSamples;
			std::atomic<int> mMaxQueueSize = { 4096 }; // The amount of samples that the queue is allowed to have
			std::atomic<bool> mVerbose = { false }; // Enable logging

			int mSpareLatency = 0; // Spare latency used to compensate for irregular supply of samples.
			int mSpareLatencyInBuffers = 0; // Spare latency as a multiple of the buffersize.
			std::atomic<int> mNewSpareLatencyInBuffers = 0; // Atomic to store values for mSpareLatencyInBuffers as a multiple of the buffersize.
			bool mSavingSpare = true;
			DirtyFlag mClearSpareBuffer;
		};

	}
}
