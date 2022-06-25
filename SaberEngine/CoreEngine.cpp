#include "CoreEngine.h"
#include "BuildConfiguration.h"

#include <SDL.h>

#include <string>


namespace SaberEngine
{
	// Static members:
	CoreEngine*		CoreEngine::coreEngine; // Assigned in constructor

	EventManager*	CoreEngine::SaberEventManager	= &EventManager::Instance();
	InputManager*	CoreEngine::SaberInputManager	= &InputManager::Instance();
	SceneManager*	CoreEngine::SaberSceneManager	= &SceneManager::Instance();
	RenderManager*	CoreEngine::SaberRenderManager	= &RenderManager::Instance();


	CoreEngine::CoreEngine(int argc, char** argv) : SaberObject("CoreEngine")
	{
		this->coreEngine = this;

		if (!ProcessCommandLineArgs(argc, argv))
		{
			exit(-1);
		}
	}


	void CoreEngine::Startup()
	{
		LOG("CoreEngine starting...");

		// Initialize SDL:
		if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
		{
			LOG_ERROR(SDL_GetError());
			exit(-1);
		}

		// Initialize SaberEngine:
		SaberEventManager->Startup();	
		SaberLogManager->Startup();
		
		SaberEventManager->Subscribe(EVENT_ENGINE_QUIT, this);

		SaberTimeManager->Startup();
		SaberInputManager->Startup();

		SaberRenderManager->Startup();

		// Must wait to start scene manager and load a scene until the renderer is called, since we need to initialize OpenGL in the RenderManager before creating shaders
		SaberSceneManager->Startup();
		bool loadedScene = SaberSceneManager->LoadScene(config.currentScene);

		// Now that the scene (and its materials/shaders) has been loaded, we can initialize the shaders
		if (loadedScene)
		{
			SaberRenderManager->Initialize();
		}		

		isRunning = true;

		return;
	}


	// Main game loop
	void CoreEngine::Run()
	{
		LOG("CoreEngine beginning main game loop!");

		// Process any events that might have occurred during startup:
		this->SaberEventManager->Update();
		this->SaberLogManager->Update();

		// Initialize game loop timing:
		this->SaberTimeManager->Update();
		double elapsed = 0.0;

		while (isRunning)
		{
			this->SaberInputManager->Update();

			// Process events
			this->SaberEventManager->Update(); // Clears SDL event queue: Must occur after any other component that listens to SDL events
			this->SaberLogManager->Update();

			this->SaberTimeManager->Update();	// We only need to call this once per loop. DeltaTime() effectively == #ms between calls to TimeManager.Update()
			elapsed += SaberTimeManager->DeltaTime();

			while (elapsed >= FIXED_TIMESTEP)
			{
				// Update components:
				this->SaberEventManager->Update(); // Clears SDL event queue: Must occur after any other component that listens to SDL events
				this->SaberLogManager->Update();

				this->SaberSceneManager->Update(); // Updates all of the scene objects

				elapsed -= FIXED_TIMESTEP;
			}
			
			this->SaberRenderManager->Update();

			this->Update();
		}
	}


	void CoreEngine::Stop()
	{
		isRunning = false;
	}


	void CoreEngine::Shutdown()
	{
		LOG("CoreEngine shutting down...");

		config.SaveConfig();
		
		// Note: Shutdown order matters!
		SaberTimeManager->Shutdown();		
		SaberInputManager->Shutdown();
		
		SaberRenderManager->Shutdown();
		
		SaberSceneManager->Shutdown();
		
		SaberEventManager->Shutdown();

		SaberLogManager->Shutdown();

		delete SaberTimeManager;
		delete SaberInputManager;
		delete SaberRenderManager;
		delete SaberSceneManager;
		delete SaberEventManager;
		delete SaberLogManager;		

		SDL_Quit();

		return;
	}


	EngineConfig const* CoreEngine::GetConfig()
	{
		return &config;
	}

	
	void CoreEngine::Update()
	{
		// Generate a quit event if the quit button is pressed:
		if (this->SaberInputManager->GetKeyboardInputState(INPUT_BUTTON_QUIT) == true)
		{
			this->SaberEventManager->Notify(new EventInfo{ EVENT_ENGINE_QUIT, this });
		}
	}


	void CoreEngine::HandleEvent(EventInfo const* eventInfo)
	{
		switch (eventInfo->type)
		{
		case EVENT_ENGINE_QUIT:
		{
			Stop();
		}			
			break;

		default:
			LOG_ERROR("ERROR: Default event generated in CoreEngine!");
			break;
		}

		return;
	}


	bool CoreEngine::ProcessCommandLineArgs(int argc, char** argv)
	{
		if (argc <= 1)
		{
			LOG_ERROR("No command line arguments received! Use \"-scene <scene name>\" to launch a scene from the .\\Scenes directory.\n\n\t\tEg. \tSaberEngine.exe -scene Sponza\n\nNote: The scene directory name and scene .FBX file must be the same");
			return false;
		}
		LOG("Processing " + to_string(argc - 1) + " command line arguments:");

		for (int i = 1; i < argc; i++) // Start at index 1 to skip the executable path
		{			
			string currentArg = string(argv[i]);			
			if (currentArg.find("-scene") != string::npos)
			{
				string parameter = string(argv[i + 1]);
				if (i < argc - 1)
				{
					LOG("\tReceived scene command: \"" + currentArg + " " + parameter + "\"");

					this->config.currentScene = parameter;
				}
				
				i++; // Eat the extra command parameter
			}
			else
			{
				LOG_ERROR("\"" + currentArg + "\" is not a recognized command!");
			}

		}

		return true;
	}
}