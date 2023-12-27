// © 2022 Adam Badke. All rights reserved.
#include "Camera.h"
#include "CastUtils.h"
#include "Config.h"
#include "Light.h"
#include "MeshConcept.h"
#include "ParameterBlock.h"
#include "PerformanceTimer.h"
#include "SceneManager.h"
#include "Transform.h"

using fr::SceneData;
using en::Config;
using re::Batch;
using re::ParameterBlock;
using fr::Transform;
using util::PerformanceTimer;
using std::shared_ptr;
using std::make_shared;
using std::string;
using glm::mat4;


namespace
{
	constexpr size_t k_initialBatchReservations = 100;
}

namespace en
{
	SceneManager* SceneManager::Get()
	{
		static std::unique_ptr<en::SceneManager> instance = std::make_unique<en::SceneManager>();
		return instance.get();
	}


	SceneManager::SceneManager()
		: m_sceneData(nullptr)
		, m_activeCameraIdx(0)
	{
	}


	void SceneManager::Startup()
	{
		LOG("SceneManager starting...");

		PerformanceTimer timer;
		timer.Start();

		// Load the scene:
		string sceneName;
		if (Config::Get()->TryGetValue<string>(en::ConfigKeys::k_sceneNameKey, sceneName))
		{
			m_sceneData = std::make_shared<SceneData>(sceneName);
		}
		else
		{
			LOG_WARNING("No scene name found to load");			
			m_sceneData = std::make_shared<SceneData>("Empty Scene");
		}

		string sceneFilePath;
		Config::Get()->TryGetValue<string>(en::ConfigKeys::k_sceneFilePathKey, sceneFilePath);

		const bool loadResult = m_sceneData->Load(sceneFilePath);
		if (!loadResult)
		{
			LOG_ERROR("Failed to load scene: %s", sceneFilePath);
			EventManager::Get()->Notify(EventManager::EventInfo{ EventManager::EngineQuit });
		}

		LOG("\nSceneManager::Startup complete in %f seconds...\n", timer.StopSec());
	}


	void SceneManager::Shutdown()
	{
		LOG("Scene manager shutting down...");

		m_sceneData = nullptr; // SceneData::Destroy is called during RenderManager shutdown
	}


	void SceneManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		// 
	}


	void SceneManager::FinalUpdate()
	{
		//
	}
}


