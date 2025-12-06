#pragma once

#include <vbanudpserver.h>
#include <vbanreceiver.h>
#include <audio/service/portaudioservice.h>

#ifdef __APPLE__
    #if !defined(__x86_64__) && !defined(_M_X64)
#define __APPLESILICON__
    #endif
#endif

namespace nap
{

    namespace audio
    {
        /**
         * Version of the VBANUDPServer that is optimized to be used in combination with napportaudio.
         * I uses the audioworkgroup threading feature on macOS.
         */
        class NAPAPI PortAudioVBANServer : public VBANUDPServer
        {
            RTTI_ENABLE(VBANUDPServer)
            
        public:
            PortAudioVBANServer(Core& core);

            // Apple silicon specific
#ifdef __APPLESILICON__

            bool start(utility::ErrorState& errorState) override;
            void stop() override;

        protected:
            void threadFunction() override;

        private:
            Slot<const audio::PortAudioServiceConfiguration::DeviceSettings&> mDeviceSettingsChangedSlot = { this, &PortAudioVBANServer::deviceSettingsChanged };

            void deviceSettingsChanged(const audio::PortAudioServiceConfiguration::DeviceSettings& settings);

            os_workgroup_t mWorkGroup;
            bool mStarted = false;
#endif

        private:
            audio::PortAudioService& mAudioService;
        };


        /**
         * Verison of the VBANReceiver that is tweaked for usage in combination with napportaudio.
         * When an audio callback is late it resets the actual latency in order to stay in sync with the sender.
         */
        class NAPAPI PortAudioVBANReceiver : public VBANReceiver
        {
        public:
            PortAudioVBANReceiver(Core& core);

            bool init(utility::ErrorState& errorState) override;
            void onDestroy() override;

            Slot<double> mLateAudioCallbackSlot = { this, &PortAudioVBANReceiver::onLateAudioCallback };
            void onLateAudioCallback(double time) { getCircularBuffer()->reset(); }

        private:
            audio::PortAudioService& mAudioService;

        };

    }

}