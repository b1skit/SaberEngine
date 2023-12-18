// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
#include "BatchManager.h"
#include "Camera.h"
#include "CastUtils.h"
#include "Config.h"
#include "Light.h"
#include "Mesh.h"
#include "ParameterBlock.h"
#include "PerformanceTimer.h"
#include "SceneManager.h"
#include "Transform.h"

using fr::SceneData;
using en::Config;
using re::Batch;
using re::ParameterBlock;
using gr::Transform;
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
		m_sceneBatches.reserve(k_initialBatchReservations);
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

		m_sceneData->PostLoadFinalize();

		LOG("\nSceneManager::Startup complete in %f seconds...\n", timer.StopSec());
	}


	void SceneManager::Shutdown()
	{
		LOG("Scene manager shutting down...");

		m_sceneData = nullptr; // SceneData::Destroy is called during RenderManager shutdown
	}


	void SceneManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		// Updateables have been ticked by the GameplayManager; Now we can update the Transforms and scene BoundsConcept
		m_sceneData->UpdateSceneBounds();
	}


	void SceneManager::FinalUpdate()
	{
		BuildSceneBatches();
	}


	void SceneManager::SetMainCameraIdx(size_t camIdx)
	{
		EventManager::Get()->Notify(EventManager::EventInfo{ EventManager::CameraSelectionChanged });

		m_activeCameraIdx = camIdx;
	}


	std::shared_ptr<gr::Camera> SceneManager::GetMainCamera() const
	{
		// TODO: This camera is accessed multiple times before the first frame is rendered (e.g. PlayerObject, various
		// graphics systems). Currently, this is fine as we currently join any loading threads before creating these
		// objects, but it may not always be the case.

		return m_sceneData->GetMainCamera(m_activeCameraIdx);
	}


	std::vector<re::Batch>& SceneManager::GetSceneBatches()
	{
		// NOTE: The caller should std::move this; m_sceneBatches must be empty for the next BuildSceneBatches call
		return m_sceneBatches;
	};


	void SceneManager::BuildSceneBatches()
	{
		// TODO: We're currently creating/destroying invariant parameter blocks each frame. This is expensive.
		// Instead, we should create a pool of PBs, and reuse by re-buffering data each frame

		SEAssert("Scene batches should be empty", m_sceneBatches.empty());

		std::vector<shared_ptr<gr::Mesh>> const& sceneMeshes = SceneManager::GetSceneData()->GetMeshes();
		if (sceneMeshes.empty())
		{
			return;
		}

		m_sceneBatches = std::move(re::BatchManager::BuildBatches(sceneMeshes));
	}
}


