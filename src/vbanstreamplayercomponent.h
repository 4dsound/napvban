/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// Nap includes
#include <nap/resourceptr.h>
#include <audio/utility/safeptr.h>

// Audio includes
#include <audio/component/audiocomponentbase.h>
#include <audio/node/controlnode.h>

// Vban includes
#include "vbancircularbuffer.h"
#include "vbanreceiver.h"

namespace nap
{
	namespace audio
	{
		// Forward declares
		class AudioService;
		class VBANStreamPlayerComponentInstance;

		/**
		 * VBANStreamPlayerComponent hooks up to a VBANReceiver and plays incoming VBAN packets.
		 * The VBAN packets must be configured to have the same samplerate and amount of channels as channels created in channel routing
		 */
		class NAPAPI VBANStreamPlayerComponent : public AudioComponentBase
		{
			RTTI_ENABLE(AudioComponentBase)
			DECLARE_COMPONENT(VBANStreamPlayerComponent, VBANStreamPlayerComponentInstance)

		public:
			/**
			 * Constructor
			 */
			VBANStreamPlayerComponent() : AudioComponentBase() { }

			/**
			 * Returns if the playback consists of 2 audio channels
			 */
			bool isStereo() const { return mChannelRouting.size() == 2; }

			// Properties
			ResourcePtr<VBANReceiver> mVBANPacketReceiver = nullptr; ///< Property: "VBANPacketReceiver" the packet receiver
			std::vector<int> mChannelRouting = { }; ///< Property: "ChannelRouting" the channel routing, must be equal to excpected channels from stream
			std::string mStreamName; ///< Property: "StreamName" the VBAN stream to listen to
		public:
		};


		/**
		 * VBANStreamPlayerComponentInstance
		 * Instance of VBANStreamPlayerComponent. Implements IVBANStreamListener interface
		 */
		class NAPAPI VBANStreamPlayerComponentInstance : public AudioComponentBaseInstance
		{
			RTTI_ENABLE(AudioComponentBaseInstance)

		public:
			VBANStreamPlayerComponentInstance(EntityInstance& entity, Component& resource)
				: AudioComponentBaseInstance(entity, resource) { }

			// Inherited from ComponentInstance
			bool init(utility::ErrorState& errorState) override;

			/**
			 * Called before deconstruction
			 * Removes listener from VBANPacketReceiver
			 */
			void onDestroy() override;

			// Inherited from AudioComponentBaseInstance
			int getChannelCount() const override { return mReader->getChannelCount(); }
			OutputPin* getOutputForChannel(int channel) override { assert(channel < mReader->getChannelCount()); return &mReader->getOutputPin(channel); }

			/**
			 * Sets streamname this VBANStreamPlayer accepts
			 * @param streamName this VBANStreamPlayer accepts
			 */
			void setStreamName(const std::string& streamName){ mStreamName = streamName; }

		private:
			SafeOwner<VBANCircularBufferReader> mReader;
			std::vector<int> mChannelRouting;
			std::string mStreamName;

			// VBANStreamPlayerComponent* mResource = nullptr; // The component's resource
			NodeManager* mNodeManager = nullptr; // The audio node manager this component's audio nodes are managed by
			AudioService* mAudioService = nullptr; // audio server
			SafePtr<VBANCircularBuffer> mCircularBuffer = nullptr; // the circular buffer used for receiving audio data
		};
	}
}
