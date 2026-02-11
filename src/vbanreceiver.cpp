#include "vbanreceiver.h"

#include <vbanutils.h>
#include <vban/vban.h>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VBANReceiver)
    RTTI_CONSTRUCTOR(nap::Core&)
	RTTI_PROPERTY("Server", &nap::VBANReceiver::mServer, nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("CircularBufferSize", &nap::VBANReceiver::mCircularBufferSize, nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace nap
{

    VBANReceiver::VBANReceiver(Core &core)
    {
        mAudioService = core.getService<audio::AudioService>();
    }


    bool VBANReceiver::init(utility::ErrorState &errorState)
    {
    	auto& nodeManager = mAudioService->getNodeManager();
    	mCircularBuffer = nodeManager.makeSafe<VBANCircularBuffer>(nodeManager, mCircularBufferSize);

    	// Register as root process
    	registerBufferProcess(mCircularBuffer.get());

    	// Register with the VBANUDPServer
    	mServer->registerListenerSlot(mPacketReceivedSlot);

        return true;
    }


    void VBANReceiver::onDestroy()
    {
        mServer->removeListenerSlot(mPacketReceivedSlot);
    	unregisterBufferProcess(mCircularBuffer.get());
    }


    void VBANReceiver::packetReceived(const VBANUDPServer::Packet &packet)
    {
		const VBanHeader *const header = (struct VBanHeader *)(&packet.data()[0]);

		// Let the circular buffer convert, deinterleave and write directly
		mCircularBuffer->write(*header, packet.size());
    }


}