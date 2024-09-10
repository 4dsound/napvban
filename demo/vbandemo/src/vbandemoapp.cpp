#include "vbandemoapp.h"
#include "audio/component/levelmetercomponent.h"

// External Includes
#include <utility/fileutils.h>
#include <nap/logger.h>
#include <inputrouter.h>
#include <rendergnomoncomponent.h>
#include <perspcameracomponent.h>
#include <audio/component/playbackcomponent.h>

#include <string>


RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::VBANDemoApp)
	RTTI_CONSTRUCTOR(nap::Core&)
RTTI_END_CLASS

namespace nap 
{
	bool VBANDemoApp::init(utility::ErrorState& error)
	{
		// Retrieve services
		mRenderService	= getCore().getService<nap::RenderService>();
		mSceneService	= getCore().getService<nap::SceneService>();
		mInputService	= getCore().getService<nap::InputService>();
		mGuiService		= getCore().getService<nap::IMGuiService>();
		mAudioService	= getCore().getService<nap::audio::PortAudioService>();

		// Fetch the resource manager
		mResourceManager = getCore().getResourceManager();

		// Get the render window
		mRenderWindow = mResourceManager->findObject<nap::RenderWindow>("Window");
		if (!error.check(mRenderWindow != nullptr, "unable to find render window with name: %s", "Window"))
			return false;

		// Get the scene that contains our entities and components
		mScene = mResourceManager->findObject<Scene>("Scene");
		if (!error.check(mScene != nullptr, "unable to find scene with name: %s", "Scene"))
			return false;

        // Get VBANReceiver Entity
        mVBANReceiverEntity = mScene->findEntity("VBANReceiverEntity");
        if (!error.check(mVBANReceiverEntity != nullptr, "unable to find entity with name: %s", "VBANReceiverEntity"))
            return false;
//        mVBANReceiverEntity = mScene->findEntity("VBANReceiverEntity:1");
//        if (!error.check(mVBANReceiverEntity != nullptr, "unable to find entity with name: %s", "VBANReceiverEntity"))
//            return false;

        mPlotSenderValues.resize(512, 0);
        mSenderTickIdx = 0;
        mAudioDeviceSettingsGui = std::make_unique<audio::AudioDeviceSettingsGui>(*getCore().getService<audio::PortAudioService>(), false);

        capFramerate(true);

		// All done!
		return true;
	}
	
	
	// Update app
	void VBANDemoApp::update(double deltaTime)
	{

		// Config file modal ID
		static constexpr char* configModalID = (char*)"Config File";
		static std::string configError;
		// Use a default input router to forward input events (recursively) to all input components in the default scene
		nap::DefaultInputRouter input_router(true);
		mInputService->processWindowEvents(*mRenderWindow, input_router, { &mScene->getRootEntity() });    
        const auto& pallete = getCore().getService<IMGuiService>()->getPalette();
		mAudioDeviceSettingsGui->drawGui();



        /**
         * Draw the windows displaying info about the VBAN sender and receiver
         *
         */

        ImGui::Begin("Reciever Count");
        if(ImGui::Button("plus"))
        {
            
            mVBANReciverE = mResourceManager->findObject<Entity>("VBANReceiverEntity");
			auto errostate = nap::utility::ErrorState();
			Entity* vbanEntity = mVBANReciverE.get();
            SpawnedEntityInstance EntityPtr = mScene->spawn(*vbanEntity, errostate);
    
            auto PlayerComponent = EntityPtr.get()->findComponent<audio::VBANStreamPlayerComponentInstance>();
            unsigned long count = mRecieverEntities.size();
            std::string newstream = std::string("vbandemo") +std::to_string(count);
            PlayerComponent->setStreamName(newstream);
            
			mRecieverEntities.push_back(EntityPtr);

            std::vector<audio::ControllerValue> newPloter = {};
            newPloter.resize(512, 0);
            std::fill(newPloter.begin(),newPloter.end(),0);
            mPlotReceiverValues.push_back(newPloter);

		}

            
        if(ImGui::Button("minus"))
        {
			auto del_entity = mRecieverEntities.back();
			del_entity = mRecieverEntities.back();
			mScene->destroy(del_entity);
			mRecieverEntities.pop_back();   
            mPlotReceiverValues.pop_back();
        }

        for (std::size_t i = 0, e = mRecieverEntities.size(); i != e; ++i) {
			// render the gui for all the vban recievers
			auto& vban_stream_player_instance = mRecieverEntities[i]->getComponent<audio::VBANStreamPlayerComponentInstance>();

			ImGui::Text("VBAN Receiver");

			ImGui::Text("Listening for VBAN packets on port:");
			ImGui::SameLine();
			ImGui::TextColored(pallete.mHighlightColor3, "%i", vban_stream_player_instance.getPortNumber());

			ImGui::Text("Listening to stream:");
			ImGui::SameLine();
			ImGui::TextColored(pallete.mHighlightColor3, "%s", vban_stream_player_instance.getStreamName().c_str());
            

    			ImGui::Spacing();
    			ImGui::Text("Received Audio (Channel 0)");
    			auto receiver_level_meter = mRecieverEntities[i]->findComponent<audio::LevelMeterComponentInstance>();
    
    						// Store new value in array
    			mPlotReceiverValues[i][mReceiverTickIdx] = receiver_level_meter->getLevel();	// save new value so it can be subtracted later
    			if (++mReceiverTickIdx == mPlotReceiverValues[i].size())			// increment current sample index
    				mReceiverTickIdx = 0;
    
    			ImGui::PlotHistogram("",
    								mPlotReceiverValues[i].data(),
    								mPlotReceiverValues[i].size(),
    								mReceiverTickIdx, nullptr, 0.0f, 0.2f,
    								ImVec2(ImGui::GetColumnWidth(), 128));
    		}

        ImGui::End();

	}
	
	
	// Render app
	void VBANDemoApp::render()
	{
		// Signal the beginning of a new frame, allowing it to be recorded.
		// The system might wait until all commands that were previously associated with the new frame have been processed on the GPU.
		// Multiple frames are in flight at the same time, but if the graphics load is heavy the system might wait here to ensure resources are available.
		mRenderService->beginFrame();

		// Begin recording the render commands for the main render window
		if (mRenderService->beginRecording(*mRenderWindow))
		{
			// Begin render pass
			mRenderWindow->beginRendering();

			// Render GUI elements
			mGuiService->draw();

			// Stop render pass
			mRenderWindow->endRendering();

			// End recording
			mRenderService->endRecording();
		}

		// Proceed to next frame
		mRenderService->endFrame();
	}
	

	void VBANDemoApp::windowMessageReceived(WindowEventPtr windowEvent)
	{
		mRenderService->addEvent(std::move(windowEvent));
	}
	
	
	void VBANDemoApp::inputMessageReceived(InputEventPtr inputEvent)
	{
		if (inputEvent->get_type().is_derived_from(RTTI_OF(nap::KeyPressEvent)))
		{
			// If we pressed escape, quit the loop
			nap::KeyPressEvent* press_event = static_cast<nap::KeyPressEvent*>(inputEvent.get());
			if (press_event->mKey == nap::EKeyCode::KEY_ESCAPE)
				quit();

			// f is pressed, toggle full-screen
			if (press_event->mKey == nap::EKeyCode::KEY_f)
				mRenderWindow->toggleFullscreen();
		}
		// Add event, so it can be forwarded on update
		mInputService->addEvent(std::move(inputEvent));
	}

	
	int VBANDemoApp::shutdown()
	{
		// Clean up
		mRecieverEntities.clear();
		mVBANReceiverEntity = nullptr;
		return 0;
	}

}
