#include "vbanreceiver.h"

#include <vbanutils.h>
#include <vban/vban.h>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VBANReceiver)
    RTTI_CONSTRUCTOR(nap::Core&)
	RTTI_PROPERTY("Server", &nap::VBANReceiver::mServer, nap::rtti::EPropertyMetaData::Required)
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
    	mCircularBuffer = nodeManager.makeSafe<VBANCircularBuffer>(nodeManager);

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
		utility::ErrorState errorState;

		// Check packet
		if (!checkPacket(errorState, &packet.data()[0], packet.size()))
		{
			mErrorMessage = errorState.toString();
			mCorrectPacketCounter = 0;
			return;
		}

		struct VBanHeader const *const hdr = (struct VBanHeader *) (&packet.data()[0]);

		// get sample rate
		int const sample_rate_format = hdr->format_SR & VBAN_SR_MASK;
		int sample_rate = 0;
		if (!utility::getSampleRateFromVBANSampleRateFormat(sample_rate, sample_rate_format, errorState))
		{
			mErrorMessage = errorState.toString();
			mCorrectPacketCounter = 0;
			return;
		}

		// get stream name, use it to forward buffers to any registered stream audio receivers
		const std::string stream_name(hdr->streamname);

		// Check if packet samplerate matches the current napaudio samplerate.
		if (sample_rate != mCircularBuffer->getSampleRate())
		{
			errorState.fail("%s: Samplerate mismatch.", stream_name.c_str());
			mErrorMessage = errorState.toString();
			mCorrectPacketCounter = 0;
			return;
		}

		// get packet meta-data
		int const nb_samples = hdr->format_nbs + 1;
		int const nb_channels = hdr->format_nbc + 1;
		uint32 const packetCounter = hdr->nuFrame;
		// Logger::debug("%s %i %i %i", stream_name.c_str(), nb_samples, nb_channels, packetCounter);
		auto const format = hdr->format_bit;
		int sample_size;
		if (format == VBAN_BITFMT_32_INT)
			sample_size = 4;
		else
			sample_size = 2;

		size_t payload_size = nb_samples * sample_size * nb_channels;

		// Resize buffers to push to players
		int float_buffer_size = int(payload_size / sample_size) / nb_channels;
		mBuffers.resize(nb_channels, float_buffer_size);

		// convert WAVE PCM multiplexed signal into floating point (SampleValue) buffers for each channel
		int pos = VBAN_HEADER_SIZE;
		if (sample_size == 4)
		{
			// 32 bit
			for (int i = 0; i < float_buffer_size; i++)
			{
				for (int channel = 0; channel < nb_channels; channel++)
				{
					unsigned char byte_1 = packet.data()[pos];
					unsigned char byte_2 = packet.data()[pos + 1];
					unsigned char byte_3 = packet.data()[pos + 2];
					unsigned char byte_4 = packet.data()[pos + 3];
					int32_t original_value =
						static_cast<int32_t>(byte_4) << 24 |
						static_cast<int32_t>(byte_3) << 16 |
						static_cast<int32_t>(byte_2) << 8 |
						(0x000000ff & byte_1);

					mBuffers[channel][i] = (float) original_value / (float) std::numeric_limits<int32_t>::max();
					pos += 4;
				}
			}
		}
		else {
			// 16 bit
			for (int i = 0; i < float_buffer_size; i++)
			{
				for (int channel = 0; channel < nb_channels; channel++)
				{
					unsigned char byte_1 = packet.data()[pos];
					unsigned char byte_2 = packet.data()[pos + 1];
					int16_t original_value = static_cast<int16_t>(byte_2) << 8 | (0x00ff & byte_1);

					mBuffers[channel][i] = (float) original_value / (float) std::numeric_limits<int16_t>::max();
					pos += 2;
				}
			}
		}

        if (mCircularBuffer->write(stream_name, packetCounter * float_buffer_size, mBuffers))
        {
        	mCorrectPacketCounter++;
        }
        else
        {
        	mCorrectPacketCounter = 0;
        	mErrorMessage = "Stream not found: " + stream_name;
        }

    }


	bool VBANReceiver::checkPacket(utility::ErrorState& errorState, nap::uint8 const* buffer, size_t size)
	{
		struct VBanHeader const* const hdr = (struct VBanHeader*)(buffer);

		if (!errorState.check(buffer != 0, "buffer is null ptr"))
			return false;

		if (!errorState.check(size > VBAN_HEADER_SIZE, "packet too small"))
			return false;

		if (!errorState.check(hdr->vban == *(int32_t*)("VBAN"), "invalid vban magic fourc"))
			return false;

		if (!errorState.check(hdr->format_bit == VBAN_BITFMT_16_INT || hdr->format_bit == VBAN_BITFMT_32_INT, "reserved format bit invalid value, only 16 or 32 bit PCM supported at this time"))
			return false;

		if (!errorState.check((hdr->format_nbc + 1) > 0, "channel count cannot be 0 or smaller"))
			return false;

		// check protocol and codec
		auto protocol = static_cast<VBanProtocol>(hdr->format_SR & VBAN_PROTOCOL_MASK);
		auto codec = static_cast<VBanCodec>(hdr->format_bit & VBAN_CODEC_MASK);

		if (protocol != VBAN_PROTOCOL_AUDIO)
		{
			switch (protocol)
			{
				case VBAN_PROTOCOL_SERIAL:
				case VBAN_PROTOCOL_TXT:
				case VBAN_PROTOCOL_UNDEFINED_1:
				case VBAN_PROTOCOL_UNDEFINED_2:
				case VBAN_PROTOCOL_UNDEFINED_3:
				case VBAN_PROTOCOL_UNDEFINED_4:
					errorState.fail("protocol not supported yet");
				default:
					errorState.fail("packet with unknown protocol");
			}

			return false;
		}
		else {
			if (!errorState.check(codec == VBAN_CODEC_PCM, "unsupported codec"))
				return false;

			if (!checkPcmPacket(errorState, buffer, size))
				return false;
		}

		return true;
	}


	bool VBANReceiver::checkPcmPacket(utility::ErrorState& errorState, const nap::uint8* buffer, size_t size)
	{
		// the packet is already a valid vban packet and buffer already checked before
		struct VBanHeader const* const hdr = (struct VBanHeader*)(buffer);
		enum VBanBitResolution const bit_resolution = static_cast<const VBanBitResolution>(hdr->format_bit & VBAN_BIT_RESOLUTION_MASK);
		int const sample_rate_format   = hdr->format_SR & VBAN_SR_MASK;

		if (!errorState.check(bit_resolution < VBAN_BIT_RESOLUTION_MAX, "invalid bit resolution"))
			return false;

		if (!errorState.check(sample_rate_format < VBAN_SR_MAXNUMBER, "invalid sample rate"))
			return false;

		return true;
	}


    bool VBANReceiver::hasErrors()
    {
        return false;
    }


    void VBANReceiver::getErrorMessage(std::string &message)
    {
        message.clear();
    }

}