// © 2022 Adam Badke. All rights reserved.
#include "Camera.h"
#include "CastUtils.h"
#include "Config.h"
#include "Light.h"
#include "MeshConcept.h"
#include "PerformanceTimer.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "Transform.h"


namespace
{
	constexpr size_t k_initialBatchReservations = 100;
}

namespace fr
{
	SceneManager* SceneManager::Get()
	{
		static std::unique_ptr<fr::SceneManager> instance = std::make_unique<fr::SceneManager>();
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

		util::PerformanceTimer timer;
		timer.Start();

		// Load the scene:
		std::string sceneName;
		if (en::Config::Get()->TryGetValue<std::string>(en::ConfigKeys::k_sceneNameKey, sceneName))
		{
			m_sceneData = std::make_shared<fr::SceneData>(sceneName);
		}
		else
		{
			LOG_WARNING("No scene name found to load");			
			m_sceneData = std::make_shared<fr::SceneData>("Empty Scene");
		}

		std::string sceneFilePath;
		en::Config::Get()->TryGetValue<std::string>(en::ConfigKeys::k_sceneFilePathKey, sceneFilePath);

		const bool loadResult = m_sceneData->Load(sceneFilePath);
		if (!loadResult)
		{
			LOG_ERROR("Failed to load scene: %s", sceneFilePath);
			en::EventManager::Get()->Notify(en::EventManager::EventInfo{ en::EventManager::EngineQuit });
		}

		LOG("\nSceneManager::Startup complete in %f seconds...\n", timer.StopSec());
	}


	void SceneManager::Shutdown()
	{
		LOG("Scene manager shutting down...");


		// NOTE: OpenGL objects must be destroyed on the render thread, so we trigger them here by recording a command
		// to call SceneData::Destroy during RenderManager shutdown
		class DestroySceneDataRenderCommand
		{
		public:
			DestroySceneDataRenderCommand(std::shared_ptr<fr::SceneData> sceneData) : m_sceneData(sceneData) {}
			~DestroySceneDataRenderCommand() { m_sceneData = nullptr; }

			static void Execute(void* cmdData)
			{
				DestroySceneDataRenderCommand* cmdPtr = reinterpret_cast<DestroySceneDataRenderCommand*>(cmdData);
				cmdPtr->m_sceneData->Destroy();
			}

			static void Destroy(void* cmdData)
			{
				DestroySceneDataRenderCommand* cmdPtr = reinterpret_cast<DestroySceneDataRenderCommand*>(cmdData);
				cmdPtr->~DestroySceneDataRenderCommand();
			}
		private:
			std::shared_ptr<fr::SceneData> m_sceneData;
		};


		re::RenderManager::Get()->EnqueueRenderCommand<DestroySceneDataRenderCommand>(
			DestroySceneDataRenderCommand(std::move(m_sceneData)));
	}


	void SceneManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		// 
	}
}


