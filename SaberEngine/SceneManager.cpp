#include <algorithm>
#include <string>

#include "SceneManager.h"
#include "CoreEngine.h"
#include "Camera.h"
#include "PlayerObject.h"

using gr::Camera;
using fr::SceneData;
using std::shared_ptr;
using std::make_shared;
using std::string;


namespace en
{
	SceneManager::SceneManager() : EngineComponent("SceneManager"),
		m_currentScene(nullptr)
	{	
	}


	void SceneManager::Startup()
	{
		LOG("SceneManager starting...");
		
		// Load the scene:
		const string sceneName = en::CoreEngine::GetConfig()->GetValue<string>("sceneName");
		m_currentScene = std::make_shared<SceneData>(sceneName);

		const string sceneFilePath = en::CoreEngine::GetConfig()->GetValue<string>("sceneFilePath");
		const bool loadResult = m_currentScene->Load(sceneFilePath);
		if (!loadResult)
		{
			CoreEngine::GetEventManager()->Notify(std::make_shared<EventManager::EventInfo const>(
				EventManager::EventInfo{ EventManager::EngineQuit, this, "Failed to load scene" }));
		}

		// Add a player object to the scene:
		shared_ptr<fr::PlayerObject> player = std::make_shared<fr::PlayerObject>(m_currentScene->GetMainCamera());
		m_currentScene->AddGameObject(player);
		LOG("Created PlayerObject using \"%s\"", m_currentScene->GetMainCamera()->GetName().c_str());
	}


	void SceneManager::Shutdown()
	{
		LOG("Scene manager shutting down...");

		m_currentScene = nullptr;
	}


	void SceneManager::Update()
	{
		for (int i = 0; i < (int)m_currentScene->GetGameObjects().size(); i++)
		{
			m_currentScene->GetGameObjects().at(i)->Update();
		}
	}


	void SceneManager::HandleEvent(shared_ptr<EventManager::EventInfo const> eventInfo)
	{
		return;
	}
}


