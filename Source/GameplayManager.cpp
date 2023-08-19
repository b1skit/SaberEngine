// © 2022 Adam Badke. All rights reserved.
#include "GameplayManager.h"
#include "PlayerObject.h"
#include "SceneManager.h"
#include "Camera.h"
#include "DebugConfiguration.h"


namespace fr
{
	using en::Updateable;
	using std::unique_ptr;
	using std::shared_ptr;
	using std::make_unique;
	using std::make_shared;
	

	GameplayManager* GameplayManager::Get()
	{
		static unique_ptr<fr::GameplayManager> instance = make_unique<fr::GameplayManager>();
		return instance.get();
	}


	void GameplayManager::Startup()
	{
		LOG("GameplayManager starting...");

		std::shared_ptr<gr::Camera> mainCam = en::SceneManager::Get()->GetMainCamera();

		// Add a player object to the scene:
		shared_ptr<fr::PlayerObject> player = make_shared<fr::PlayerObject>(mainCam);

		m_updateables.emplace_back(player);
		LOG("Created PlayerObject using \"%s\"", mainCam->GetName().c_str());
	}


	void GameplayManager::Shutdown()
	{
		LOG("GameplayManager shutting down...");

		m_updateables.clear();
	}


	void GameplayManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		for (shared_ptr<Updateable> updateable : m_updateables)
		{
			updateable->Update(stepTimeMs);
		}
	}
}