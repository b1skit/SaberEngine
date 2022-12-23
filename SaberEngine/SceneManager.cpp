#include <algorithm>
#include <string>

#include "Config.h"
#include "Math.h"
#include "ParameterBlock.h"
#include "PerformanceTimer.h"
#include "SceneManager.h"
#include "Transform.h"


namespace
{
	constexpr size_t k_initialBatchReservations = 100;
}

namespace en
{
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


	SceneManager* SceneManager::Get()
	{
		static std::unique_ptr<en::SceneManager> instance = std::make_unique<en::SceneManager>();
		return instance.get();
	}


	SceneManager::SceneManager() 
		: m_sceneData(nullptr)
	{
		m_sceneBatches.reserve(k_initialBatchReservations);
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


	void SceneManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		// Tick Updateables:
		for (int i = 0; i < (int)m_sceneData->GetUpdateables().size(); i++)
		{
			m_sceneData->GetUpdateables().at(i)->Update(stepTimeMs);
		}

		// Recompute Scene Bounds. This also recomputes all Transforms in a DFS ordering
		m_sceneData->RecomputeSceneBounds();
	}


	void SceneManager::FinalUpdate()
	{
		BuildSceneBatches();
	}


	std::vector<re::Batch>& SceneManager::GetSceneBatches()
	{
		// NOTE: The caller should std::move this; m_sceneBatches must be empty for the next BuildSceneBatches call
		return m_sceneBatches;
	};


	void SceneManager::BuildSceneBatches()
	{
		SEAssert("Scene batches should be empty", m_sceneBatches.empty());

		std::vector<shared_ptr<gr::Mesh>> const& sceneMeshes = SceneManager::GetSceneData()->GetMeshes();
		if (sceneMeshes.empty())
		{
			return;
		}

		// Build batches from scene meshes:
		// TODO: Build this by traversing the scene hierarchy once a scene graph is implemented
		std::vector<std::pair<Batch, Transform*>> unmergedBatches;
		for (shared_ptr<gr::Mesh> mesh : sceneMeshes)
		{
			for (shared_ptr<re::MeshPrimitive> const meshPrimitive : mesh->GetMeshPrimitives())
			{
				unmergedBatches.emplace_back(std::pair<Batch, Transform*>(
					{
						meshPrimitive.get(),
						meshPrimitive->MeshMaterial().get(),
						meshPrimitive->MeshMaterial()->GetShader().get()
					},
					mesh->GetTransform()));
			}
		}

		// Sort the batches:
		std::sort(
			unmergedBatches.begin(),
			unmergedBatches.end(),
			[](std::pair<Batch, Transform*> const& a, std::pair<Batch, Transform*> const& b)
			-> bool { return (a.first.GetDataHash() > b.first.GetDataHash()); }
		);

		// Assemble a list of merged batches:
		size_t unmergedIdx = 0;
		do
		{
			// Add the first batch in the sequence to our final list:
			m_sceneBatches.emplace_back(unmergedBatches[unmergedIdx].first);
			const uint64_t curBatchHash = m_sceneBatches.back().GetDataHash();

			// Find the index of the last batch with a matching hash in the sequence:
			const size_t instanceStartIdx = unmergedIdx++;
			while (unmergedIdx < unmergedBatches.size() &&
				unmergedBatches[unmergedIdx].first.GetDataHash() == curBatchHash)
			{
				unmergedIdx++;
			}
			const size_t numInstances = unmergedIdx - instanceStartIdx;

			// Get the first model matrix:
			std::vector<mat4> modelMatrices;
			modelMatrices.reserve(numInstances);
			modelMatrices.emplace_back(unmergedBatches[instanceStartIdx].second->GetGlobalMatrix(Transform::TRS));

			// Append the remaining batches in the sequence:
			for (size_t instanceIdx = instanceStartIdx + 1; instanceIdx < unmergedIdx; instanceIdx++)
			{
				m_sceneBatches.back().IncrementBatchInstanceCount();

				modelMatrices.emplace_back(unmergedBatches[instanceIdx].second->GetGlobalMatrix(Transform::TRS));
			}

			// Construct PB of model transform matrices:
			shared_ptr<ParameterBlock> instancedMeshParams = ParameterBlock::CreateFromArray(
				"InstancedMeshParams",
				modelMatrices.data(),
				sizeof(mat4),
				numInstances,
				ParameterBlock::PBType::SingleFrame);
			// TODO: We're currently creating/destroying these parameter blocks each frame. This is expensive. Instead,
			// we should create a pool of PBs, and reuse by re-buffering data each frame

			m_sceneBatches.back().AddBatchParameterBlock(instancedMeshParams);
		} while (unmergedIdx < unmergedBatches.size());
	}
}


