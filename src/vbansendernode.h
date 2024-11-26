/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// Std includes
#include <atomic>

// Vban includes
#include <vban/vban.h>
#include <vban/vbanstreamencoder.h>

// Nap includes
#include <udpclient.h>

// Audio includes
#include <audio/core/audionode.h>
#include <audio/utility/dirtyflag.h>
#include <audio/utility/safeptr.h>
#include <audio/core/process.h>
#include <audio/core/audionodemanager.h>

namespace nap
{

	namespace audio
	{

		/**
		 * Node that allows registering one or more non-node processor objects that incoming audio will be passed to.
		 */
		class NAPAPI VBANSenderNode : public Node
		{
			RTTI_ENABLE(Node)

		public:
			VBANSenderNode(NodeManager& nodeManager);

			virtual ~VBANSenderNode();

			/**
			 * Connect incoming audio to be precessed by the processors here.
			 */
			MultiInputPin inputs = {this};

			void setUDPClient(UDPClient* client) { getNodeManager().enqueueTask([&, client](){ mUDPClient = client; }); }
			void setStreamName(const std::string& name) { mEncoder.setStreamName(name); }
			void sendPacket(const std::vector<char>& data)
			{
				UDPPacket packet(reinterpret_cast<const std::vector<uint8>&>(data));
				mUDPClient->send(std::move(packet));
			}

		private:
			// Wraps result of MultiInputPin::pull so that it can be passed to VBANStreamEncoder.
			class PullResultWrapper
			{
			public:
				PullResultWrapper() = default;
				const std::vector<SampleValue>& operator[](int i) const { return *mPullResult[i]; }
				std::vector<std::vector<SampleValue>*>& get() { return mPullResult; }

			private:
				std::vector<std::vector<SampleValue>*> mPullResult;
			};

		private:
			// Inherited from Node
			void process() override;
			void sampleRateChanged(float) override;

			UDPClient* mUDPClient = nullptr;
			vban::VBANStreamEncoder<VBANSenderNode> mEncoder;
			PullResultWrapper mInputPullResult;
			std::vector<nap::uint8> mData;
		};

	}

}
