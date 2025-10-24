#pragma once

#include <vbancircularbuffer.h>
#include <vbanudpserver.h>

#include <audio/service/audioservice.h>
#include <nap/resourceptr.h>

namespace nap
{

    class NAPAPI VBANReceiver : public Resource
    {
        RTTI_ENABLE(Resource)

    public:
        ResourcePtr<VBANUDPServer> mServer = nullptr; ///< Property: 'Server' Pointer to the VBAN UDP server receiving the packets
        bool mManuallyRegisterAudioProcess = false; ///< Property: 'ManuallyRegisterAudioProcess' Indicates whether the user takes responsibility of registering the circular buffer's process with a parent process. By default this is false and the circular buffer is registered as root process.

        VBANReceiver(Core& core);
        ~VBANReceiver();
        bool init(utility::ErrorState& errorState) override;
        void onDestroy() override;

        /**
         * @return True if incoming packets are not being handled correctly.
         */
        bool hasErrors();

        /**
         * @message When hasErrors() returns true, this is set to the current error message.
         */
        void getErrorMessage(std::string& message);

        audio::SafePtr<VBANCircularBuffer> getCircularBuffer() { return mCircularBuffer.get(); }

    private:
        Slot<const VBANUDPServer::Packet&> mPacketReceivedSlot = { this, &VBANReceiver::packetReceived };
        void packetReceived(const VBANUDPServer::Packet& packet);

        bool checkPacket(utility::ErrorState& errorState, nap::uint8 const* buffer, size_t size);
        bool checkPcmPacket(utility::ErrorState& errorState, nap::uint8 const* buffer, size_t size);

        audio::SafeOwner<VBANCircularBuffer> mCircularBuffer;
        audio::MultiSampleBuffer mBuffers;
        audio::AudioService* mAudioService = nullptr;
    	std::string mErrorMessage;
    	std::atomic<int> mCorrectPacketCounter = { 0 };
    };

}