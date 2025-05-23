/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */


#pragma once
#include <concurrentqueue.h>
#include <nap/numeric.h>
#include <nap/signalslot.h>
#include <udppacket.h>

// Local includes
#include <asio/buffer.hpp>

#include <udpadapter.h>
#include <vban/vban.h>


namespace nap
{


	/**
	 * VBAN specific variation on the UDPServer.
	 */
	class NAPAPI VBANUDPServer final : public Device
	{
		RTTI_ENABLE(Device)

	public:
		using Packet = std::vector<uint8_t>;

	public:
		VBANUDPServer();
		virtual ~VBANUDPServer();

		/**
		 * Connects a listener slot to the packetReceived signal. Thread-Safe
		 * @param slot the slot that will be invoked when a packet is received
		 */
		void registerListenerSlot(Slot<const Packet&>& slot);

		/**
		 * Disconnects a listener slot from the packetReceived signal. Thread-Safe
		 * @param slot the slot that will be disconnected
		 */
		void removeListenerSlot(Slot<const Packet&>& slot);

		int mPort 						= 13251;		///< Property: 'Port' the port the server socket binds to
		std::string mIPAddress			= "";	        ///< Property: 'IP Address' local ip address to bind to, if left empty will bind to any local address
		int mReceiveBufferSize = 1000000;				///< Property: 'ReceiveBufferSize'

		// Inherited from Device
		bool start(utility::ErrorState& errorState) override final;
		void stop() override final;

	protected:
		/**
		 * packet received signal will be dispatched on the thread this UDPServer is registered to, see UDPThread
		 */
		Signal<const Packet&> packetReceived;

	private:
		void process();
		bool handleAsioError(const std::error_code& errorCode, utility::ErrorState& errorState, bool& success);

		// Server specific ASIO implementation
		class Impl;
		std::unique_ptr<Impl> mImpl;

		std::unique_ptr<std::thread> mThread = nullptr;
		Packet mPacket; // The packet data is being reused to avoid unnecessary reallocations and copies.
		std::atomic<bool> mRunning;
		std::mutex mMutex;
	};

} // nap

