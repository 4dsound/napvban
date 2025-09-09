/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "samplequeueplayernode.h"
#include "mathutils.h"

#include <nap/logger.h>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::audio::SampleQueuePlayerNode)
	RTTI_PROPERTY("audioOutput", &nap::audio::SampleQueuePlayerNode::audioOutput, nap::rtti::EPropertyMetaData::Embedded)
RTTI_END_CLASS

namespace nap
{

	namespace audio
	{

		SampleQueuePlayerNode::SampleQueuePlayerNode(NodeManager& manager) : Node(manager)
		{
			mSamples.resize(getBufferSize());
		}


		void SampleQueuePlayerNode::queueSamples(const float* samples, size_t numSamples)
		{
			// check if queue size is exceeded, if so throw a warning, if not queue the samples
			if(mQueue.size_approx() <= mMaxQueueSize + mSpareLatency)
			{
				if (!mQueue.enqueue_bulk(samples, numSamples))
				{
					nap::Logger::error("%s: Failed to allocate memory for queue buffer",  std::string(get_type().get_name()).c_str());
				}
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


		void SampleQueuePlayerNode::process()
		{
			// get output buffer
			auto& outputBuffer = getOutputBuffer(audioOutput);

			// Update spare latency
			auto newSpareLatencyInBuffers = mNewSpareLatencyInBuffers.load();
			if (newSpareLatencyInBuffers != mSpareLatencyInBuffers || mClearSpareBuffer.check())
			{
				Logger::debug("SampleQueuePlayerNode: Spare latency changed or spare buffer cleared");
				mSpareLatencyInBuffers = newSpareLatencyInBuffers;
				while (mQueue.try_dequeue_bulk(mSamples.data(), mSamples.size()) > 0);
				mSpareLatency = mSpareLatencyInBuffers * getBufferSize();
				mSavingSpare = true;
				std::fill(outputBuffer.begin(), outputBuffer.end(), 0.0f);
				return;
			}

			// get buffer size
			const int available_samples = mQueue.size_approx();

			if (mSavingSpare)
			{
				if (available_samples < getBufferSize() + mSpareLatency)
				{
					std::fill(outputBuffer.begin(), outputBuffer.end(), 0.0f);
					return;
				}
				else {
					mSavingSpare = false;
				}
			}

			// dequeue the samples from the queue and fill the rest with the outputbuffer
			if (mQueue.try_dequeue_bulk(mSamples.begin(), getBufferSize()))
			{
				// fill the output buffer
				std::memcpy(&outputBuffer.data()[0], mSamples.data(), getBufferSize() * sizeof(SampleValue));
			}
			else {
				// not enough samples in queue, fill with silence
				Logger::debug("SampleQueuePlayerNode: Not enough samples in queue");
				mSavingSpare = true;
				std::fill(outputBuffer.begin(), outputBuffer.end(), 0.0f);
			}
		}


		void SampleQueuePlayerNode::bufferSizeChanged(int bufferSize)
		{
			mClearSpareBuffer.set();
		}


		void SampleQueuePlayerNode::sampleRateChanged(float sampleRate)
		{
			mClearSpareBuffer.set();
		}

	}
}
