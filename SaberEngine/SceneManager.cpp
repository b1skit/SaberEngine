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
		
		LoadScene(en::CoreEngine::GetConfig()->SceneName());

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


	void SceneManager::LoadScene(string const& sceneName)
	{
		if (sceneName.empty())
		{
			SEAssert("No scene name received. Did you forget to use the \"-scene theSceneName\" command line "
				"argument?", !sceneName.empty());
			
			CoreEngine::GetEventManager()->Notify(
				std::make_shared<EventManager::EventInfo const>( EventManager::EventInfo
				{ 
					EventManager::EngineQuit,
					this, 
					"No scene name received"
				}));
			return;
		}

		if (m_currentScene != nullptr)
		{
			LOG("Unloading existing scene");
			m_currentScene = nullptr;
		}		

		m_currentScene = std::make_shared<SceneData>(sceneName);

		m_currentScene->Load();

		return;
	}
}


