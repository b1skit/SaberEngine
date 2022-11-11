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

using gr::TextureTargetSet;
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
	std::unique_ptr<RenderManager> RenderManager::m_instance = nullptr;
	RenderManager* RenderManager::Get()
	{
		if (m_instance == nullptr)
		{
			m_instance = std::make_unique<RenderManager>();
		}
		return m_instance.get();
	}


	RenderManager::RenderManager() :
		m_defaultTargetSet(nullptr),
		m_pipeline("Main pipeline")
	{
		m_sceneBatches.reserve(k_initialBatchReservations);
	}


	RenderManager::~RenderManager()
	{
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
		m_defaultTargetSet->CreateColorTargets(); // Default framebuffer has no texture targets
	}


	void RenderManager::Shutdown()
	{
		LOG("Render manager shutting down...");
	}


	void RenderManager::Update()
	{
		// Build batches:
		BuildSceneBatches();

		// Update the graphics systems:
		for (size_t gs = 0; gs < m_graphicsSystems.size(); gs++)
		{
			m_graphicsSystems[gs]->PreRender(m_pipeline.GetPipeline()[gs]);
		}

		// Update/buffer param blocks:
		m_paramBlockManager.UpdateParamBlocks();

		// API-specific rendering loop:
		platform::RenderManager::Render(*this);

		// End of frame cleanup:
		m_paramBlockManager.EndOfFrame();
	}


	void RenderManager::BuildSceneBatches()
	{
		m_sceneBatches.clear();

		fr::SceneData const * const sceneData = SceneManager::GetSceneData();

		std::vector<shared_ptr<re::MeshPrimitive>> const& sceneMeshes = sceneData->GetMeshPrimitives();
		if (sceneMeshes.empty())
		{
			return;
		}

		// Build batches from scene meshes:
		// TODO: Build this by traversing the scene hierarchy once a scene graph is implemented
		std::vector<Batch> unmergedBatches;
		for (shared_ptr<re::MeshPrimitive const> const meshPrimitive : sceneData->GetMeshPrimitives())
		{
			unmergedBatches.emplace_back(
				meshPrimitive.get(), meshPrimitive->MeshMaterial().get(), meshPrimitive->MeshMaterial()->GetShader().get());
		}

		// Sort the batches:
		std::sort(
			unmergedBatches.begin(),
			unmergedBatches.end(),
			[](Batch const& a, Batch const& b) -> bool { return (a.GetDataHash() > b.GetDataHash()); }
		);

		// Assemble a list of merged batches:
		size_t unmergedIdx = 0;
		do
		{
			// Add the first batch in the sequence to our final list:
			m_sceneBatches.emplace_back(unmergedBatches[unmergedIdx]);
			const uint64_t curBatchHash = m_sceneBatches.back().GetDataHash();

			// Find the index of the last batch with a matching hash in the sequence:
			const size_t instanceStartIdx = unmergedIdx++;
			while (unmergedIdx < unmergedBatches.size() && 
				unmergedBatches[unmergedIdx].GetDataHash() == curBatchHash)
			{
				unmergedIdx++;
			}
			const size_t numInstances = unmergedIdx - instanceStartIdx;

			// Get the first model matrix:
			std::vector<mat4> modelMatrices;		
			modelMatrices.reserve(numInstances);
			modelMatrices.emplace_back(
				unmergedBatches[instanceStartIdx].GetBatchMesh()->GetOwnerTransform()->GetGlobalMatrix(Transform::TRS));

			// Append the remaining batches in the sequence:
			for (size_t instanceIdx = instanceStartIdx + 1; instanceIdx < unmergedIdx; instanceIdx++)
			{
				m_sceneBatches.back().IncrementBatchInstanceCount();

				modelMatrices.emplace_back(
					unmergedBatches[instanceIdx].GetBatchMesh()->GetOwnerTransform()->GetGlobalMatrix(Transform::TRS));
			}

			// Construct PB of model transform matrices:
			shared_ptr<ParameterBlock> instancedMeshParams = ParameterBlock::Create(
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
		platform::RenderManager::Initialize(*this);
	}


	template <typename T>
	shared_ptr<GraphicsSystem> RenderManager::GetGraphicsSystem()
	{
		// TODO: A linear search isn't optimal here, but there aren't many graphics systems in practice so ok for now
		for (size_t i = 0; i < m_graphicsSystems.size(); i++)
		{
			if (dynamic_cast<T*>(m_graphicsSystems[i].get()) != nullptr)
			{
				return m_graphicsSystems[i];
			}
		}

		SEAssertF("Graphics system not found");
		return nullptr;
	}
	// Explicitely instantiate our templates so the compiler can link them from the .cpp file:
	template shared_ptr<GraphicsSystem> RenderManager::GetGraphicsSystem<GBufferGraphicsSystem>();
	template shared_ptr<GraphicsSystem> RenderManager::GetGraphicsSystem<DeferredLightingGraphicsSystem>();
	template shared_ptr<GraphicsSystem> RenderManager::GetGraphicsSystem<ShadowsGraphicsSystem>();
	template shared_ptr<GraphicsSystem> RenderManager::GetGraphicsSystem<SkyboxGraphicsSystem>();
	template shared_ptr<GraphicsSystem> RenderManager::GetGraphicsSystem<BloomGraphicsSystem>();
	template shared_ptr<GraphicsSystem> RenderManager::GetGraphicsSystem<TonemappingGraphicsSystem>();
}


