#pragma once

#include <vbanudpserver.h>
#include <audio/service/portaudioservice.h>

namespace nap
{

    namespace audio
    {
    
        class NAPAPI portaudiovbanserver : public VBANUDPServer
        {
            RTTI_ENABLE(VBANUDPServer)
            
        public:
            portaudiovbanserver(Core& core);

#if not defined(__x86_64__) && not defined(_M_X64) && defined(__APPLE__) // Apple silicon specific

            bool start(utility::ErrorState& errorState) override;
            void stop() override;

        protected:
            void threadFunction() override;

        private:
            Slot<const audio::PortAudioServiceConfiguration::DeviceSettings&> mDeviceSettingsChangedSlot = { this, &portaudiovbanserver::deviceSettingsChanged };

            void deviceSettingsChanged(const audio::PortAudioServiceConfiguration::DeviceSettings& settings);

            os_workgroup_t mWorkGroup;
            bool mStarted = false;
#endif

        private:
            audio::PortAudioService& mAudioService;
        };

    }

}