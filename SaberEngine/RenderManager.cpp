#include <string>
#include <unordered_map>
#include <algorithm>
#include <utility>

#include "RenderManager.h"
#include "RenderManager_Platform.h"
#include "Config.h"
#include "SceneManager.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_Tonemapping.h"
#include "Batch.h"
#include "PerformanceTimer.h"

using re::TextureTargetSet;
using gr::GBufferGraphicsSystem;
using gr::DeferredLightingGraphicsSystem;
using gr::GraphicsSystem;
using gr::ShadowsGraphicsSystem;
using gr::SkyboxGraphicsSystem;
using gr::BloomGraphicsSystem;
using gr::TonemappingGraphicsSystem;
using gr::Transform;
using re::MeshPrimitive;
using re::Batch;
using en::Config;
using en::SceneManager;
using util::PerformanceTimer;
using std::shared_ptr;
using std::make_shared;
using std::string;
using std::vector;
using glm::mat4;


namespace
{
	constexpr size_t k_initialBatchReservations = 100;
}


namespace re
{
	RenderManager* RenderManager::Get()
	{
		static std::unique_ptr<re::RenderManager> instance = std::make_unique<re::RenderManager>();
		return instance.get();
	}


	RenderManager::RenderManager() :
		m_defaultTargetSet(nullptr),
		m_pipeline("Main pipeline")
	{
		m_sceneBatches.reserve(k_initialBatchReservations);
	}


	RenderManager::~RenderManager()
	{
		m_pipeline.Destroy();
		m_graphicsSystems.clear();
		m_sceneBatches.clear();

		m_defaultTargetSet = nullptr;

		// NOTE: We must destroy anything that holds a parameter block before the ParameterBlockAllocator is destroyed, 
		// as parameter blocks call the ParameterBlockAllocator in their destructor
		m_paramBlockAllocator.Destroy();

		// Do this in the destructor so we can still read any final OpenGL error messages before it is destroyed
		m_context.Destroy();
	}


	void RenderManager::Startup()
	{
		LOG("RenderManager starting...");
		m_context.Create();

		// Default target set:
		LOG("Creating default texure target set");
		m_defaultTargetSet = make_shared<TextureTargetSet>("Default target");
		m_defaultTargetSet->Viewport() = 
		{ 
			0, 
			0, 
			(uint32_t)Config::Get()->GetValue<int>("windowXRes"),
			(uint32_t)Config::Get()->GetValue<int>("windowYRes")
		};
		// Note: Default framebuffer has no texture targets
	}


	void RenderManager::Shutdown()
	{
		LOG("Render manager shutting down...");
	}


	void RenderManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		// Build batches:
		BuildSceneBatches();

		// Update the graphics systems:
		for (size_t gs = 0; gs < m_graphicsSystems.size(); gs++)
		{
			m_graphicsSystems[gs]->PreRender(m_pipeline.GetPipeline()[gs]);
		}

		// Update/buffer param blocks:
		m_paramBlockAllocator.UpdateMutableParamBlocks();

		// API-specific rendering loop:
		platform::RenderManager::Render(*this);
		platform::RenderManager::RenderImGui(*this);

		// Present the final frame:
		m_context.SwapWindow();

		// Cleanup:
		EndOfFrame();
	}


	void RenderManager::EndOfFrame()
	{
		for (StagePipeline& stagePipeline : m_pipeline.GetPipeline())
		{
			stagePipeline.EndOfFrame();
		}

		m_sceneBatches.clear(); // Need to make sure we're not holding any PB's before the next call
		
		m_paramBlockAllocator.EndOfFrame();
	}


	void RenderManager::BuildSceneBatches()
	{
		SEAssert("Scene batches have not been cleared", m_sceneBatches.empty());

		fr::SceneData const* const sceneData = SceneManager::GetSceneData();

		std::vector<shared_ptr<gr::Mesh>> const& sceneMeshes = sceneData->GetMeshes();
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
				ParameterBlock::UpdateType::Immutable,
				ParameterBlock::Lifetime::SingleFrame);
			// TODO: We're currently creating/destroying these parameter blocks each frame. This is expensive. Instead,
			// we should create a pool of PBs, and reuse by re-buffering data each frame

			m_sceneBatches.back().AddBatchParameterBlock(instancedMeshParams);
		} while (unmergedIdx < unmergedBatches.size());
	}


	void RenderManager::Initialize()
	{
		LOG("RenderManager Initializing...");
		PerformanceTimer timer;
		timer.Start();

		platform::RenderManager::Initialize(*this);

		LOG("\nRenderManager::Initialize complete in %f seconds...\n", timer.StopSec());
	}


	void RenderManager::EnqueueImGuiCommand(std::shared_ptr<en::Command> command)
	{
		m_imGuiCommands.emplace(command);
	}
}


