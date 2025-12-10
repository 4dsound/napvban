#pragma once

#include <vbancircularbuffer.h>
#include <vbanudpserver.h>

#include <audio/service/audioservice.h>
#include <nap/resourceptr.h>

namespace nap
{
    /**
     * Receives incoming VBAN packets from a VBANUDPServer and writes their audio data into a VBANCircularBuffer.
     * The circular buffer can be grabbed and read from by VBANCircularBufferReader.
     * Also streams can be added and removed from the VBANCircularBuffer.
     */
    class NAPAPI VBANReceiver : public Resource
    {
        RTTI_ENABLE(Resource)

    public:
        ResourcePtr<VBANUDPServer> mServer = nullptr; ///< Property: 'Server' Pointer to the VBAN UDP server receiving the packets
        int mCircularBufferSize = 8192; ///< Property: 'CircularBufferSize' Size of the circular buffer

        /**
         * Constructor
         * @param core The core instance
         */
        VBANReceiver(Core& core);

        // Inherited from Resource
        bool init(utility::ErrorState& errorState) override;
        void onDestroy() override;

        /**
         * @return The VBANCircularBuffer process that can be used to add and remove streams and read from by a VBANCircularBufferReader.
         */
        audio::SafePtr<VBANCircularBuffer> getCircularBuffer() { return mCircularBuffer.get(); }

    private:
        /**
         * Normally the VBANCircularBuffer process is registered as root process with the NodeManager.
         * This behaviour van be overwritten in order to register with a custom parent process for example.
         * @param process The VBANCircularBuffer to register.
         */
        virtual void registerBufferProcess(audio::SafePtr<audio::Process> process) { getNodeManager().registerRootProcess(process); }

        /**
         * Normally the VBANCircularBuffer process is registered as root process with the NodeManager.
         * This behaviour van be overwritten in order to register with a custom parent process for example.
         * @param process The VBANCircularBuffer to unregister.
         */
        virtual void unregisterBufferProcess(audio::SafePtr<audio::Process> process) { getNodeManager().unregisterRootProcess(process); }

    protected:
        audio::NodeManager& getNodeManager() { return mAudioService->getNodeManager(); }

    private:
        // Slot for receiving VBAN packets from the VBANUDPServer
        Slot<const VBANUDPServer::Packet&> mPacketReceivedSlot = { this, &VBANReceiver::packetReceived };
        void packetReceived(const VBANUDPServer::Packet& packet);

        audio::SafeOwner<VBANCircularBuffer> mCircularBuffer; // The VBANCircularBuffer to write packet data into
        audio::AudioService* mAudioService = nullptr;
    };

}