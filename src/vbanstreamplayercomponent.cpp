
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "vbanstreamplayercomponent.h"
#include "udpclient.h"

// Nap includes
#include <entity.h>
#include <nap/core.h>

// Audio includes
#include <audio/service/audioservice.h>

// RTTI
RTTI_BEGIN_CLASS(nap::audio::VBANStreamPlayerComponent)
		RTTI_PROPERTY("VBANPacketReceiver", &nap::audio::VBANStreamPlayerComponent::mVBANPacketReceiver, nap::rtti::EPropertyMetaData::Required)
		RTTI_PROPERTY("ChannelRouting", &nap::audio::VBANStreamPlayerComponent::mChannelRouting, nap::rtti::EPropertyMetaData::Default)
		RTTI_PROPERTY("StreamName", &nap::audio::VBANStreamPlayerComponent::mStreamName, nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::audio::VBANStreamPlayerComponentInstance)
		RTTI_CONSTRUCTOR(nap::EntityInstance &, nap::Component &)
RTTI_END_CLASS

namespace nap
{

	namespace audio
	{
		void VBANStreamPlayerComponentInstance::onDestroy()
		{
            mVbanReceiver->removeStreamListener(this);
		}


		bool VBANStreamPlayerComponentInstance::init(utility::ErrorState& errorState)
		{
            // acquire resources
			mResource = getComponent<VBANStreamPlayerComponent>();
			mVbanReceiver = mResource->mVBANPacketReceiver.get();
			mStreamName = mResource->mStreamName;

			// acquire audio service
			mAudioService = getEntityInstance()->getCore()->getService<AudioService>();

			mNodeManager = &mAudioService->getNodeManager();
			mChannelRouting = mResource->mChannelRouting;

            // get sample rate
            mSampleRate = static_cast<int>(mNodeManager->getSampleRate());

            // create buffer player for each channel
			auto mBufferPlayer = mNodeManager->makeSafe<SampleQueuePlayerNode>(*mNodeManager);
			mBufferPlayer->setChannelCount(mChannelRouting.size());

            // register to the packet receiver
            mVbanReceiver->registerStreamListener(this);

			return true;
		}


		bool VBANStreamPlayerComponentInstance::pushBuffers(const MultiSampleBuffer& buffers, utility::ErrorState& errorState)
		{
			if (buffers.getChannelCount() == getChannelCount())
			{
				mBufferPlayer->queueSamples(buffers);
				return true;
			}
			else {
				errorState.fail("Received %i buffers but expected %i", buffers.getChannelCount(), getChannelCount());
				return false;
			}
		}


		void VBANStreamPlayerComponentInstance::setLatency(int latency)
		{
			mBufferPlayer->setLatency(latency);
			mBufferPlayer->setMaxQueueSize(latency * 2);
		}


		void VBANStreamPlayerComponentInstance::clearSpareBuffers()
		{
			mBufferPlayer->clearSpareBuffer();
		}

	}
}
