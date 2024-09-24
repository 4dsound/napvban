//
// Created by Abhilash Balaji on 18/09/2024.
//

#include "vbanudpserver.h"
// ASIO Includes
#include <asio/ip/udp.hpp>
#include <asio/io_service.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

// External includes
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#include <asio/io_service.hpp>
#include <asio/system_error.hpp>
#include <nap/logger.h>

#include <thread>
#include <vban/vban.h>

RTTI_BEGIN_CLASS(nap::VBANUDPServer)
    RTTI_PROPERTY("Port",			        &nap::VBANUDPServer::mPort,			                nap::rtti::EPropertyMetaData::Default)
    RTTI_PROPERTY("IP Address",		        &nap::VBANUDPServer::mIPAddress,	                nap::rtti::EPropertyMetaData::Default)
    RTTI_PROPERTY("Multicast Groups",		&nap::VBANUDPServer::mMulticastGroups,	            nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS
using namespace asio::ip;

namespace nap {
    class VBANUDPServer::Impl {
    public:
        explicit Impl(asio::io_context& service) : mIOContext(service){}

        // ASIO
        asio::io_context& 			mIOContext;
        asio::ip::udp::endpoint 	mRemoteEndpoint;
        asio::ip::udp::socket       mSocket{ mIOContext };
    };

        VBANUDPServer::VBANUDPServer() = default;
    VBANUDPServer::~VBANUDPServer() = default;



    bool nap::VBANUDPServer::onStart(nap::utility::ErrorState &errorState) {


        mImpl = std::make_unique<VBANUDPServer::Impl>(getIOContext());

        bool init_success = false;

        asio::error_code errorCode;
        mImpl->mSocket.open(udp::v4(),errorCode);
        mImpl->mSocket.set_option(asio::ip::udp::socket::receive_buffer_size(mRecvBufSize));
        if(handleAsioError(errorCode,errorState,init_success))
            return init_success;

        asio::ip::address address;
        if(mIPAddress.empty()) {
            address = asio::ip::address_v4::any();
        }
        else {
            address = asio::ip::make_address(mIPAddress,errorCode);
            if(handleAsioError(errorCode,errorState,init_success))
                return init_success;
        }


        nap::Logger::info(*this,"Listening at port %i",mPort);
        mImpl->mSocket.bind(udp::endpoint(address,mPort),errorCode);
        if(handleAsioError(errorCode,errorState,init_success))
            return init_success;
        for(const auto& multicast_group : mMulticastGroups)
        {
            auto multicast_address = make_address(multicast_group, errorCode);
            if (handleAsioError(errorCode, errorState, init_success))
                return init_success;

            mImpl->mSocket.set_option(multicast::join_group(multicast_address), errorCode);
            if (handleAsioError(errorCode, errorState, init_success))
                return init_success;
        }

        if (!UDPAdapter::init(errorState))
            return false;

        //change nic net.link.generic.system.rcvq_maxlen

        return true;

    }

    void nap::VBANUDPServer::onStop()
    {
        UDPAdapter::onDestroy();

        asio::error_code asio_error_code;
        mImpl->mSocket.close(asio_error_code);

        if(asio_error_code)
        {
            nap::Logger::error(*this, asio_error_code.message());
        }

        // explicitly delete socket
        mImpl = nullptr;
    }

    void VBANUDPServer::changeRecvBufSize(int newBufSize) {
        if(mRecvBufSize != newBufSize) {
            mRecvBufSize = newBufSize;
            mImpl->mSocket.set_option(asio::ip::udp::socket::receive_buffer_size(mRecvBufSize));
        }
    }


    void nap::VBANUDPServer::onProcess() {
        asio::error_code asio_error_code;
        mBuffer.resize(VBAN_DATA_MAX_SIZE);
            if(const uint len = mImpl->mSocket.receive(asio::buffer(mBuffer)); len > 0) {
                assert(len <= VBAN_DATA_MAX_SIZE);
                const UDPPacket packet(std::move(mBuffer));

                std::lock_guard<std::mutex> lock(mMutex);
                packetReceived.trigger(packet);
            }

            if(asio_error_code)
                nap::Logger::error(*this, asio_error_code.message());

    }


    void nap::VBANUDPServer::registerListenerSlot(Slot<const nap::UDPPacket &>& slot)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        packetReceived.connect(slot);
    }

    void nap::VBANUDPServer::removeListenerSlot(nap::Slot<const nap::UDPPacket &> &slot) {
        std::lock_guard<std::mutex> lock(mMutex);
        packetReceived.disconnect(slot);
    }
}