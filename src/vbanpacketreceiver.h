/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

// Internal includes
#include <nap/resource.h>
#include <udpserver.h>
#include <udppacket.h>
#include <utility/threading.h>

#include "vbanudpserver.h"

namespace nap
{

	/**
	 * Derive from this class to handle an incoming VBAN audio stream.
	 */
	class NAPAPI IVBANStreamListener
	{
	public:
		virtual ~IVBANStreamListener() = default;

		/**
		 * Has to be overridden to handle incoming audio data for the stream
		 * @param buffers multichannel audio buffer containing audio for each channel in the stream
		 * @param errorState contains possible errors while handling audio buffers, like channel mismatches.
		 * @return True on success
		 */
		virtual bool pushBuffers(const std::vector<std::vector<float>>& buffers, utility::ErrorState& errorState) = 0;

		/**
		 * Sets additional latency used to compensate for packets arriving late.
		 * This latency needs to be implemented by the stream listener.
		 * @param value Latency specified in number of audio buffers of the current audio system. (usually NodeManager::getBufferSize())
		 */
		virtual void setLatency(int value) = 0;

		/**
		 * CLears the spare buffers in the SampleQueuePlayerNodes.
		 */
		virtual void clearSpareBuffers() = 0;

		/**
		 * @return Has to return the name of the VBAN audio stream that this receiver will handle.
		 */
		virtual const std::string& getStreamName() = 0;

		/**
		 * Returns sample rate used by listener
		 * @return sample rate used by listener
		 */
		virtual int getSampleRate() const = 0;
	};


	/**
	 * Resource that listens to incoming VBAN UDP packets on an UDPServer object.
	 * The VBANPacketReceiver parses the packets and dispatches them to different IVBANStreamAudioReceiver objects for each stream.
	 */
	class NAPAPI VBANPacketReceiver final : public Resource
	{
		RTTI_ENABLE(Resource)

	public:
		// Inherited from Resource
		virtual bool init(utility::ErrorState& errorState);

		/**
		 * Register a new receiver for a certain stream
		 * @param listener IVBANStreamListener object that handles incoming VBAN packets for a VBAN stream
		 */
		void registerStreamListener(IVBANStreamListener* listener);

		/**
		 * Unregister an existing receiver
		 * @param listener IVBANStreamListener object that handles incoming VBAN packets for a VBAN stream
		 */
		void removeStreamListener(IVBANStreamListener* listener);

		/**
		 * Set the latency of the receiver as a multiple of the current buffersize.
		 * @param value Latency as a multiple of the buffersize.
		 */
		void setLatency(int value);

		/**
		 * Clears the spare buffers in the SampleQueuePlayerNodes that are used to correct for packet drifting.
		 */
		void clearSpareBuffers();

		/**
		 * @return Return additional latency as a multiple of the current buffer size.
		 */
		int getLatency() const { return mLatency; }

		/**
		 * @return True if incoming packets are not being handled correctly.
		 */
		bool hasErrors();

		/**
		 * @message When hasErrors() returns true, this is set to the current error message.
		 */
		void getErrorMessage(std::string& message);
		
		/**
		 * @return The number of listeners.
		 */
		int getStreamListenerCount();

	public:
		ResourcePtr<VBANUDPServer> mServer = nullptr; ///< Property: 'Server' Pointer to the VBAN UDP server receiving the packets

	protected:
		Slot<const VBANUDPServer::Packet&> mPacketReceivedSlot = { this, &VBANPacketReceiver::packetReceived };
		void packetReceived(const VBANUDPServer::Packet& packet);

	private:
		bool checkPacket(utility::ErrorState& errorState, nap::uint8 const* buffer, size_t size);
		bool checkPcmPacket(utility::ErrorState& errorState, nap::uint8 const* buffer, size_t size);

	private:
		std::mutex mReceiverMutex;
		std::vector<IVBANStreamListener*> mListeners;
		std::map<IVBANStreamListener*, uint32> mPacketCounters; // Packet counter for each stream
		std::vector<std::vector<float>> mBuffers; // Here as to not reallocate them for every received packet
		int mLatency = 1;
		std::atomic<int> mCorrectPacketCounter = { 0 };
		std::atomic<int> mReceiverCount = { 0 };
		std::string mErrorMessage;
	};


}
