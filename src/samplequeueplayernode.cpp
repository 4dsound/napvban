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


		void SampleQueuePlayerNode::process()
		{
			// get buffer size
			const int available_samples = mQueue.size_approx();
			// get output buffer
			auto& outputBuffer = getOutputBuffer(audioOutput);

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

			if (mSavingSpare == false && available_samples == 0)
			{
				mSavingSpare = true;
				std::fill(outputBuffer.begin(), outputBuffer.end(), 0.0f);
				return;
			}

			if (available_samples < getBufferSize())
			{
				std::fill(outputBuffer.begin(), outputBuffer.end(), 0.0f);
				return;
			}

			// dequeue the samples from the queue and fill the rest with the outputbuffer
			if (mQueue.try_dequeue_bulk(mSamples.begin(), getBufferSize()))
			{
				// fill the output buffer
				std::memcpy(&outputBuffer.data()[0], mSamples.data(), getBufferSize() * sizeof(SampleValue));
			}
			else {
				// no samples in queue, fill with silence
				if(mVerbose)
					nap::Logger::warn("%s: Not enough samples in queue", std::string(get_type().get_name()).c_str());
				std::fill(outputBuffer.begin(), outputBuffer.end(), 0.0f);
			}
		}

		void SampleQueuePlayerNode::bufferSizeChanged(int bufferSize)
		{
			mSamples.resize(getBufferSize());
		}


	}
}
