#include <algorithm>
#include <string>

#include "SceneManager.h"
#include "Config.h"
#include "EventManager.h"
#include "Camera.h"
#include "PlayerObject.h"
#include "Light.h"

using gr::Camera;
using gr::Light;
using fr::SceneData;
using en::Config;
using en::EventManager;
using std::shared_ptr;
using std::make_shared;
using std::string;


namespace en
{
	std::unique_ptr<SceneManager> SceneManager::m_instance = nullptr;
	SceneManager* SceneManager::Get()
	{
		if (m_instance == nullptr)
		{
			m_instance = std::make_unique<SceneManager>();
		}
		return m_instance.get();
	}


	SceneManager::SceneManager() : m_sceneData(nullptr)
	{
	}


	void SceneManager::Startup()
	{
		LOG("SceneManager starting...");
		
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

		// Add a player object to the scene:
		shared_ptr<fr::PlayerObject> player = std::make_shared<fr::PlayerObject>(m_sceneData->GetMainCamera());
		m_sceneData->AddUpdateable(player);
		LOG("Created PlayerObject using \"%s\"", m_sceneData->GetMainCamera()->GetName().c_str());
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


