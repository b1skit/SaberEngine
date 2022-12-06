#include <algorithm>
#include <string>

#include "SceneManager.h"
#include "Config.h"
#include "PerformanceTimer.h"

using fr::SceneData;
using en::Config;
using util::PerformanceTimer;
using std::shared_ptr;
using std::make_shared;
using std::string;


namespace en
{
	SceneManager* SceneManager::Get()
	{
		static std::unique_ptr<en::SceneManager> instance = std::make_unique<en::SceneManager>();
		return instance.get();
	}


	SceneManager::SceneManager() : m_sceneData(nullptr)
	{
	}


	void SceneManager::Startup()
	{
		LOG("SceneManager starting...");

		PerformanceTimer timer;
		timer.Start();
		
		// Load the scene:
		const string sceneName = Config::Get()->GetValue<string>("sceneName");
		m_sceneData = std::make_shared<SceneData>(sceneName);

		const string sceneFilePath = Config::Get()->GetValue<string>("sceneFilePath");
		const bool loadResult = m_sceneData->Load(sceneFilePath);
		if (!loadResult)
		{
			LOG_ERROR("Failed to load scene: %s", sceneFilePath);
			EventManager::Get()->Notify(EventManager::EventInfo{ EventManager::EngineQuit});
		}

		LOG("\nSceneManager::Startup complete in %f seconds...\n", timer.StopSec());
	}


	void SceneManager::Shutdown()
	{
		LOG("Scene manager shutting down...");

		m_sceneData = nullptr;
	}


	void SceneManager::Update(const double stepTimeMs)
	{
		// Tick Updateables:
		for (int i = 0; i < (int)m_sceneData->GetUpdateables().size(); i++)
		{
			m_sceneData->GetUpdateables().at(i)->Update(stepTimeMs);
		}
	}
}


