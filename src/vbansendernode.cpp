/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <vbansendernode.h>
#include <vbanstreamsendercomponent.h>
#include <vban/vban.h>
#include <vbanutils.h>

#include <audio/core/audionodemanager.h>

#include <nap/logger.h>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::audio::VBANSenderNode)
		RTTI_PROPERTY("input", &nap::audio::VBANSenderNode::inputs, nap::rtti::EPropertyMetaData::Embedded)
RTTI_END_CLASS


namespace nap
{
	namespace audio
	{

		VBANSenderNode::VBANSenderNode(NodeManager& nodeManager, vban::SharedDirtyFlag& sharedDirtyFlag) : Node(nodeManager), mEncoder(*this, sharedDirtyFlag)
		{
			mInputPullResult.get().reserve(2);
			sampleRateChanged(nodeManager.getSampleRate());
			mEncoder.setActive(true);
		}


		VBANSenderNode::~VBANSenderNode()
		{
		}


		void VBANSenderNode::process()
		{
			if (mUDPClient == nullptr)
				return;

			// get output buffers
			inputs.pull(mInputPullResult.get());

			// Update channel count
			auto channelCount = mInputPullResult.get().size();
			if (channelCount != mEncoder.getChannelCount())
				mEncoder.setChannelCount(channelCount);

			mEncoder.process(mInputPullResult, channelCount, getBufferSize());
		}


		void VBANSenderNode::sampleRateChanged(float sampleRate)
		{
			// acquire sample rate format
			utility::ErrorState errorState;
			nap::uint8 format;
			if (!utility::getVBANSampleRateFormatFromSampleRate(format,
																static_cast<int>(sampleRate),
																errorState))
				nap::Logger::error(errorState.toString().c_str());
			else
				mEncoder.setSampleRateFormat(format);
		}

	}
}
