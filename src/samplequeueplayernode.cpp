/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "samplequeueplayernode.h"
#include "mathutils.h"

#include <nap/logger.h>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::audio::SampleQueuePlayerNode)
RTTI_END_CLASS

namespace nap
{

	namespace audio
	{

		SampleQueuePlayerNode::SampleQueuePlayerNode(NodeManager& manager) : Node(manager)
		{
		}


		void SampleQueuePlayerNode::setChannelCount(int channelCount)
		{
			// Remove old outputs and disconnect connections
			mOutputPins.clear();

			// Create new pins
			for (auto channel = 0; channel < channelCount; channel++)
				mOutputPins.emplace_back(std::make_unique<OutputPin>(this));

			clearQueue();

			mSamples.resize(channelCount * getBufferSize(), 0.f);
		}


		void SampleQueuePlayerNode::queueSamples(const std::vector<std::vector<float>> &samples)
		{
			int numSamples = samples[0].size();

			// check if queue size is exceeded, if so throw a warning, if not queue the samples
			if (mQueue.size_approx() <= (mMaxQueueSize + mSpareLatency) * mOutputPins.size())
			{
				// Enqueue interleaved
				for (auto i = 0; i < numSamples; i++)
					for (auto channel = 0; channel < samples.size(); channel++)
						mQueue.enqueue(samples[channel][i]);
				// if (!mQueue.enqueue_bulk(samples, numSamples))
				// {
				// 	nap::Logger::error("%s: Failed to allocate memory for queue buffer",  std::string(get_type().get_name()).c_str());
				// }
			}
			else {
				if(mVerbose)
					nap::Logger::warn("%s: Dropping samples because buffer is getting to big", std::string(get_type().get_name()).c_str());
			}
		}


		void SampleQueuePlayerNode::setLatency(int numberOfBuffers)
		{
			mNewSpareLatencyInBuffers.store(numberOfBuffers);
		}


		void SampleQueuePlayerNode::clearSpareBuffer()
		{
			clearQueue(); // Clear the sample queue immediately
			mClearSpareBuffer.set();
		}


		void SampleQueuePlayerNode::process()
		{
			// Update spare latency
			auto newSpareLatencyInBuffers = mNewSpareLatencyInBuffers.load();
			if (newSpareLatencyInBuffers != mSpareLatencyInBuffers || mClearSpareBuffer.check())
			{
				Logger::debug("SampleQueuePlayerNode: Spare latency changed or spare buffer cleared");
				mSpareLatencyInBuffers = newSpareLatencyInBuffers;
				mSpareLatency = mSpareLatencyInBuffers * getBufferSize();
				mSavingSpare = true;
				fillOutputBuffers(0.f);
				return;
			}

			// get queue buffer size
			const int available_samples = mQueue.size_approx();

			if (mSavingSpare)
			{
				if (available_samples < (getBufferSize() + mSpareLatency) * mOutputPins.size())
				{
					fillOutputBuffers(0.f);
					return;
				}
				else {
					mSavingSpare = false;
				}
			}

			// dequeue the samples from the queue and fill the rest with the outputbuffer
			int dequeueCount =  mQueue.try_dequeue_bulk(mSamples.begin(), mSamples.size());
			if (dequeueCount == mSamples.size())
			{
				// fill the output buffers deinterleaving
				int i = 0;
				for (int frame = 0; frame < getBufferSize(); frame++)
				{
					for (int channel = 0; channel < mOutputPins.size(); channel++)
						getOutputBuffer(*mOutputPins[channel])[frame] = mSamples[i++];
				}
			}
			else {
				// not enough samples in queue, fill with silence
				Logger::debug("SampleQueuePlayerNode: Not enough samples in queue");
				mSavingSpare = true;
				fillOutputBuffers(0.f);
			}
		}


		void SampleQueuePlayerNode::bufferSizeChanged(int bufferSize)
		{
			mSamples.resize(mOutputPins.size() * getBufferSize(), 0.f);
			clearQueue();
			mClearSpareBuffer.set();
		}


		void SampleQueuePlayerNode::sampleRateChanged(float sampleRate)
		{
			mClearSpareBuffer.set();
		}


		void SampleQueuePlayerNode::clearQueue()
		{
			while (mQueue.try_dequeue_bulk(mSamples.data(), mSamples.size()) > 0);
		}


		void SampleQueuePlayerNode::fillOutputBuffers(float value)
		{
			for (auto& pin : mOutputPins)
			{
				auto& outputBuffer = getOutputBuffer(*pin);
				std::fill(outputBuffer.begin(), outputBuffer.end(), value);
			}
		}

	}
}
