#pragma once

#include <vbanudpserver.h>
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

    }

}