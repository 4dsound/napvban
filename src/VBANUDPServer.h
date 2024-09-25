//
// Created by Abhilash Balaji on 18/09/2024.
//


#pragma once
#include <concurrentqueue.h>
#include <nap/numeric.h>
#include <nap/signalslot.h>

// Local includes
#include <asio/buffer.hpp>

#include <udpadapter.h>
#include <vban/vban.h>

#include "udpadapter.h"
#include "udppacket.h"

namespace nap {

class NAPAPI VBANUDPServer final:public UDPAdapter{
    RTTI_ENABLE(UDPAdapter)
public:
    VBANUDPServer();
    virtual ~VBANUDPServer();

    void changeRecvBufSize(int newBufSize);

    /**
 * Connects a listener slot to the packetReceived signal. Thread-Safe
 * @param slot the slot that will be invoked when a packet is received
 */
    void registerListenerSlot(Slot<const UDPPacket&>& slot);

    /**
 * Disconnects a listener slot from the packetReceived signal. Thread-Safe
 * @param slot the slot that will be disconnected
 */
    void removeListenerSlot(Slot<const UDPPacket&>& slot);

    int mPort 						= 13251;		///< Property: 'Port' the port the server socket binds to
    std::string mIPAddress			= "";	        ///< Property: 'IP Address' local ip address to bind to, if left empty will bind to any local address
    std::vector<std::string> mMulticastGroups;      ///< Property: 'Multicast Groups' multicast groups to join
    bool mIsRecieveing = false;
protected:
    /**
 * packet received signal will be dispatched on the thread this UDPServer is registered to, see UDPThread
 */
    Signal<const UDPPacket&> packetReceived;

    /**
     * Called when server socket needs to be created
     * @param errorState The error state
     * @return: true on success
     */
    bool onStart(utility::ErrorState& errorState) final;

    /**
 * Called when socket needs to be closed
 */
    void onStop() final;

    /**
     * The process function
     */
    void onProcess() final;

private:
    // Server specific ASIO implementation
    class Impl;
    std::unique_ptr<Impl> mImpl;
    // mutex
    std::mutex mMutex;
    std::vector<uint8> mBuffer;
    int mRecvBufSize = VBAN_DATA_MAX_SIZE * 10;

};

} // nap

