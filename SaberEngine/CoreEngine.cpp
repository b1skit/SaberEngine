#include <string>

#include "CoreEngine.h"
#include "DebugConfiguration.h"
#include "Platform.h"

using std::shared_ptr;
using std::make_shared;
using std::string;


namespace en
{
	// Static members:
	en::EngineConfig					CoreEngine::m_config;

	CoreEngine*							CoreEngine::m_coreEngine	= nullptr;
	std::shared_ptr<en::EventManager>	CoreEngine::m_eventManager	= nullptr;
	std::shared_ptr<en::InputManager>	CoreEngine::m_inputManager	= nullptr;
	std::shared_ptr<en::SceneManager>	CoreEngine::m_sceneManager	= nullptr;
	std::shared_ptr<re::RenderManager>	CoreEngine::m_renderManager	= nullptr;


	CoreEngine::CoreEngine(int argc, char** argv) : en::SaberObject("CoreEngine"),
		m_FixedTimeStep(1000.0 / 120.0),
		m_isRunning(false),
		m_logManager(make_shared<en::LogManager>()),
		m_timeManager(make_shared<en::TimeManager>())
	{
		m_coreEngine = this;

		if (!ProcessCommandLineArgs(argc, argv))
		{
			exit(-1);
		}
	}


	void CoreEngine::Startup()
	{
		LOG("CoreEngine starting...");

		// Initialize manager singletons:
		m_eventManager	= std::make_shared<en::EventManager>();
		m_inputManager	= std::make_shared<en::InputManager>();
		m_sceneManager	= std::make_shared<en::SceneManager>();
		m_renderManager	= std::make_shared<re::RenderManager>();

		// Start managers:
		m_eventManager->Startup();
		m_logManager->Startup();

		m_eventManager->Subscribe(en::EventManager::EngineQuit, this);

		m_timeManager->Startup();

		m_renderManager->Startup();	// Initializes SDL events and video subsystems

		// For some reason, this needs to be called after the SDL video subsystem (!) has been initialized:
		m_inputManager->Startup();

		// Must wait to start scene manager and load a scene until the renderer is called, since we need to initialize
		// OpenGL in the RenderManager before creating shaders
		m_sceneManager->Startup();

		// Now that the scene (and its materials/shaders) has been loaded, we can initialize the shaders
		m_renderManager->Initialize();

		m_isRunning = true;

		return;
	}


	// Main game loop
	void CoreEngine::Run()
	{
		LOG("CoreEngine beginning main game loop!");

		// Process any events that might have occurred during startup:
		m_eventManager->Update();
		m_logManager->Update();

		// Initialize game loop timing:
		m_timeManager->Update();
		double elapsed = 0.0;

		while (m_isRunning)
		{
			m_inputManager->Update();

			// Process events
			m_eventManager->Update(); // Clears SDL event queue: Must occur after any other component that listens to SDL events
			m_logManager->Update();

			m_timeManager->Update();	// We only need to call this once per loop. DeltaTime() effectively == #ms between calls to TimeManager.Update()
			elapsed += m_timeManager->DeltaTime();

			while (elapsed >= m_FixedTimeStep)
			{
				// Update components:
				m_eventManager->Update(); // Clears SDL event queue: Must occur after any other component that listens to SDL events
				m_logManager->Update();

				m_sceneManager->Update(); // Updates all of the scene objects

				elapsed -= m_FixedTimeStep;
			}
			
			m_renderManager->Update();

			Update();
		}
	}


	void CoreEngine::Stop()
	{
		m_isRunning = false;
	}


	void CoreEngine::Shutdown()
	{
		LOG("CoreEngine shutting down...");

		m_config.SaveConfig();
		
		// Note: Shutdown order matters!
		m_timeManager->Shutdown();		
		m_inputManager->Shutdown();
		m_renderManager->Shutdown();		
		m_sceneManager->Shutdown();		
		m_eventManager->Shutdown();
		m_logManager->Shutdown();

		m_inputManager = nullptr;
		m_renderManager = nullptr;
		m_sceneManager = nullptr;
		m_eventManager = nullptr;

		return;
	}


	en::EngineConfig const* CoreEngine::GetConfig()
	{
		return &m_config;
	}

	
	void CoreEngine::Update()
	{
		// Generate a quit event if the quit button is pressed:
		if (m_inputManager->GetKeyboardInputState(en::InputButton_Quit) == true)
		{
			m_eventManager->Notify(
				std::make_shared<en::EventManager::EventInfo const>(en::EventManager::EventInfo
				{
					en::EventManager::EngineQuit,
					this,
					"Core Engine Quit"
				}));
		}
	}


	void CoreEngine::HandleEvent(std::shared_ptr<en::EventManager::EventInfo const> eventInfo)
	{
		switch (eventInfo->m_type)
		{
		case en::EventManager::EngineQuit:
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
		const int numTokens = argc - 1; // -1, as 1st arg is program name
		LOG("Processing %d command line tokens...", numTokens);

		for (int i = 1; i < argc; i++)
		{			
			string currentArg = string(argv[i]);			
			if (currentArg.find("-scene") != string::npos)
			{
				if (i < argc - 1) // -1 as we need to peek ahead
				{
					const int nextArg = i + 1;
					const string parameter = string(argv[nextArg]);

					LOG("\tReceived scene command: \"%s %s\"", currentArg.c_str(), parameter.c_str());

					m_config.SceneName() = parameter;
				}
				else
				{
					LOG_ERROR("Received \"-scene\" token, but no matching scene name");
					return false;
				}
				
				i++; // Eat the token
			}
			else
			{
				LOG_ERROR("\"%s\" is not a recognized command!", currentArg.c_str());
			}

		}

		return true;
	}
}