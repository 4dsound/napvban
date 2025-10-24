
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
			mCircularBuffer->removeStream(mStreamName);
		}


		bool VBANStreamPlayerComponentInstance::init(utility::ErrorState& errorState)
		{
            // acquire resources
			auto resource = getComponent<VBANStreamPlayerComponent>();
			mCircularBuffer = resource->mVBANPacketReceiver->getCircularBuffer();
			mStreamName = resource->mStreamName;

			// acquire audio service
			mAudioService = getEntityInstance()->getCore()->getService<AudioService>();

			mNodeManager = &mAudioService->getNodeManager();
			mChannelRouting = resource->mChannelRouting;

            // create buffer player for each channel
			mReader = mNodeManager->makeSafe<VBANCircularBufferReader>(*mNodeManager);
			mReader->init(mCircularBuffer, mStreamName, mChannelRouting.size());

            // register to the packet receiver
            mCircularBuffer->addStream(mStreamName, mChannelRouting.size());

			return true;
		}


		void VBANStreamPlayerComponentInstance::setLatency(float latency)
		{
			mCircularBuffer->setLatency(latency);
		}


	}
}
