#include "portaudiovbanserver.h"

#ifdef __APPLESILICON__
    #include "pa_mac_core.h"
#endif

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::audio::PortAudioVBANServer)
    RTTI_CONSTRUCTOR(nap::Core&)
RTTI_END_CLASS

namespace nap
{

    namespace audio
    {

#ifdef __APPLESILICON__

        PortAudioVBANServer::PortAudioVBANServer(Core &core) : VBANUDPServer(), mAudioService(*core.getService<PortAudioService>())
        {
            if (mAudioService.isActive())
            {
                PaError error = PaMacCore_GetOSWorkgroup(mAudioService.getCurrentOutputDeviceIndex(), &mWorkGroup);
                assert(error == paNoError);
            }
            mAudioService.beforeOpenStream.connect(mDeviceSettingsChangedSlot);
        }


        bool PortAudioVBANServer::start(utility::ErrorState &errorState)
        {
            if (mWorkGroup == nullptr)
                return true; // Don't fail when there is no audio workgroup. An audio device can be selected later by the user.

            mStarted = VBANUDPServer::start(errorState);
            return mStarted;
        }


        void PortAudioVBANServer::stop()
        {
            if (mStarted)
                VBANUDPServer::stop();
        }


        void PortAudioVBANServer::threadFunction()
        {
            auto threadWorkgroup = mWorkGroup;
            os_workgroup_join_token_s joinToken;
            auto result = os_workgroup_join(threadWorkgroup, &joinToken);
            assert(result == 0);

            workLoop();

            os_workgroup_leave(threadWorkgroup, &joinToken);
            assert(result == 0);
        }


        void PortAudioVBANServer::deviceSettingsChanged(const audio::PortAudioServiceConfiguration::DeviceSettings &settings)
        {
            auto device = mAudioService.getCurrentOutputDeviceIndex();
            if (device < 0)
                device = mAudioService.getCurrentInputDeviceIndex();
            PaError error = PaMacCore_GetOSWorkgroup(device, &mWorkGroup);
            if (error == paNoError)
            {
                stop();
                utility::ErrorState errorState;
                bool result = start(errorState);
                assert(result);
            }
        }

#else
        PortAudioVBANServer::PortAudioVBANServer(Core &core) : VBANUDPServer(), mAudioService(*core.getService<PortAudioService>())
        {

        }

#endif

    }

}