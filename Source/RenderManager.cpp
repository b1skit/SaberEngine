// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
#include "Config.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_Tonemapping.h"
#include "PerformanceTimer.h"
#include "RenderManager.h"
#include "RenderManager_Platform.h"
#include "SceneManager.h"


namespace re
{
	using en::Config;
	using en::SceneManager;
	using gr::BloomGraphicsSystem;
	using gr::DeferredLightingGraphicsSystem;
	using gr::GBufferGraphicsSystem;
	using gr::GraphicsSystem;
	using gr::ShadowsGraphicsSystem;
	using gr::SkyboxGraphicsSystem;
	using gr::TonemappingGraphicsSystem;
	using gr::Transform;
	using re::Batch;
	using re::MeshPrimitive;
	using util::PerformanceTimer;
	using std::shared_ptr;
	using std::make_shared;
	using std::string;
	using std::vector;
	using glm::mat4;


	RenderManager* RenderManager::Get()
	{
		static std::unique_ptr<re::RenderManager> instance = std::make_unique<re::RenderManager>();
		return instance.get();
	}


	RenderManager::RenderManager()
		: m_renderPipeline("Main pipeline")
		, m_renderFrameNum(0)
	{
		m_vsyncEnabled = en::Config::Get()->GetValue<bool>("vsync");
	}


	void RenderManager::Lifetime(std::barrier<>* copyBarrier)
	{
		// Synchronized startup: Blocks main thread until complete
		m_startupLatch[static_cast<size_t>(SyncType::ReleaseWorker)].arrive_and_wait();
		Startup();
		m_startupLatch[static_cast<size_t>(SyncType::ReleaseCommander)].arrive_and_wait();

		// Synchronized initialization: Blocks main thread until complete
		m_initializeLatch[static_cast<size_t>(SyncType::ReleaseWorker)].arrive_and_wait();
		Initialize();
		m_initializeLatch[static_cast<size_t>(SyncType::ReleaseCommander)].arrive_and_wait();


		EngineThread::ThreadUpdateParams updateParams{};

		m_isRunning = true;
		while (m_isRunning)
		{
			// Blocks until a new update params is received, or the EngineThread has been signaled to stop
			const bool doUpdate = GetUpdateParams(updateParams);
			if (!doUpdate)
			{
				break;
			}
			m_renderFrameNum = updateParams.m_frameNum;

			// Copy stage: Blocks other threads until complete
			PreUpdate(m_renderFrameNum);
			const std::barrier<>::arrival_token& copyArrive = copyBarrier->arrive();

			Update(m_renderFrameNum, updateParams.m_elapsed);
		}

		// Synchronized shutdown: Blocks main thread until complete
		m_shutdownLatch[static_cast<size_t>(SyncType::ReleaseWorker)].arrive_and_wait();
		Shutdown();
		m_shutdownLatch[static_cast<size_t>(SyncType::ReleaseCommander)].arrive_and_wait();
	}


	void RenderManager::Startup()
	{
		LOG("RenderManager starting...");
		m_context.Create();
		en::EventManager::Get()->Subscribe(en::EventManager::InputToggleVSync, this);
	}
	
	
	void RenderManager::Initialize()
	{
		LOG("RenderManager Initializing...");
		PerformanceTimer timer;
		timer.Start();

		// Build our platform-specific graphics systems:
		platform::RenderManager::Initialize(*this);

		// Create each graphics system in turn:
		vector<shared_ptr<GraphicsSystem>>::iterator gsIt;
		for (gsIt = m_graphicsSystems.begin(); gsIt != m_graphicsSystems.end(); gsIt++)
		{
			(*gsIt)->Create(m_renderPipeline.AddNewStagePipeline((*gsIt)->GetName()));
		}
		SEAssert("Render pipeline should contian an entry for each graphics system", 
			m_renderPipeline.GetNumberGraphicsSystems() == m_graphicsSystems.size());

		m_context.GetParameterBlockAllocator().ClosePermanentPBRegistrationPeriod();

		CreateAPIResources();

		LOG("\nRenderManager::Initialize complete in %f seconds...\n", timer.StopSec());
	}


	void RenderManager::PreUpdate(uint64_t frameNum)
	{
		// Copy frame data:
		SEAssert("Render batches should be empty", m_renderBatches.empty());
		m_renderBatches = std::move(SceneManager::Get()->GetSceneBatches());
		// TODO: Create a BatchManager object (owned by the SceneManager) that can handle batch double-buffering

		// Update the graphics systems:
		for (size_t gs = 0; gs < m_graphicsSystems.size(); gs++)
		{
			m_graphicsSystems[gs]->PreRender(m_renderPipeline.GetStagePipeline()[gs]);
		}

		m_context.GetParameterBlockAllocator().SwapBuffers(frameNum);
	}


	void RenderManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		HandleEvents();

		// Create any new resources that have been loaded since the last frame:
		CreateAPIResources();

		// Update/buffer param blocks:
		m_context.GetParameterBlockAllocator().BufferParamBlocks();

		// API-specific rendering loop:
		platform::RenderManager::Render(*this);
		platform::RenderManager::RenderImGui(*this);

		// Present the final frame:
		m_context.Present();

		EndOfFrame(); // Clear batches, process pipeline and parameter block allocator EndOfFrames
	}


	void RenderManager::EndOfFrame()
	{
		m_renderBatches.clear();

		for (StagePipeline& stagePipeline : m_renderPipeline.GetStagePipeline())
		{
			stagePipeline.EndOfFrame();
		}
		
		m_context.GetParameterBlockAllocator().EndOfFrame();
	}


	void RenderManager::Shutdown()
	{
		LOG("Render manager shutting down...");
		
		// API-specific destruction:
		platform::RenderManager::Shutdown(*this);

		// NOTE: OpenGL objects must be destroyed on the render thread, so we trigger them here
		en::SceneManager::GetSceneData()->Destroy();
		gr::Material::DestroyMaterialLibrary();
		re::Sampler::DestroySamplerLibrary();

		m_renderPipeline.Destroy();
		m_graphicsSystems.clear();

		// Clear the new object queues:
		{
			std::lock_guard<std::mutex> lock(m_newShaders.m_mutex);
			m_newShaders.m_newObjects.clear();
		}
		{
			std::lock_guard<std::mutex> lock(m_newMeshPrimitives.m_mutex);
			m_newMeshPrimitives.m_newObjects.clear();
		}

		// Need to do this here so the CoreEngine's Window can be destroyed
		m_context.Destroy();
	}


	void RenderManager::EnqueueImGuiCommand(std::shared_ptr<en::Command> command)
	{
		m_imGuiCommands.emplace(command);
	}


	void RenderManager::HandleEvents()
	{
		while (HasEvents())
		{
			en::EventManager::EventInfo eventInfo = GetEvent();

			switch (eventInfo.m_type)
			{
			case en::EventManager::EventType::InputToggleVSync:
			{
				m_vsyncEnabled = !m_vsyncEnabled;
				m_context.GetSwapChain().SetVSyncMode(m_vsyncEnabled);
				LOG("VSync %s", m_vsyncEnabled ? "enabled" : "disabled");
			}
			break;
			default:
			{
				SEAssertF("Unexpected event type received");
			}
			break;
			}
		}
	}

	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::Shader> newObject)
	{
		const size_t nameID = newObject->GetNameID(); // Shaders are required to have unique names

		std::lock_guard<std::mutex> lock(m_newShaders.m_mutex);

		SEAssert("Found a shader with the same name, but a different raw pointer. This suggests a duplicate Shader, "
			"which should not be possible",
			m_newShaders.m_newObjects.find(nameID) == m_newShaders.m_newObjects.end() ||
			m_newShaders.m_newObjects.at(nameID).get() == newObject.get());

		m_newShaders.m_newObjects.insert({ nameID, newObject });
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::MeshPrimitive> newObject)
	{
		const size_t dataHash = newObject->GetDataHash(); // MeshPrimitives can have duplicate names

		std::lock_guard<std::mutex> lock(m_newMeshPrimitives.m_mutex);

		SEAssert("Found an object with the same data hash. This suggests a duplicate object exists and has not been "
			"detected, or an object is being added twice, which should not happen",
			m_newMeshPrimitives.m_newObjects.find(dataHash) == m_newMeshPrimitives.m_newObjects.end());

		m_newMeshPrimitives.m_newObjects.insert({ dataHash, newObject });
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::Texture> newObject)
	{
		const size_t nameID = newObject->GetNameID(); // Textures are required to have unique names

		std::lock_guard<std::mutex> lock(m_newTextures.m_mutex);

		SEAssert("Found an object with the same data hash. This suggests a duplicate object exists and has not been "
			"detected, or an object is being added twice, which should not happen",
			m_newTextures.m_newObjects.find(nameID) == m_newTextures.m_newObjects.end());

		m_newTextures.m_newObjects.insert({ nameID, newObject });
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::Sampler> newObject)
	{
		// Samplers are (currently) required to have unique names (e.g. "WrapLinearLinear")
		const size_t nameID = newObject->GetNameID();

		std::lock_guard<std::mutex> lock(m_newSamplers.m_mutex);

		SEAssert("Found an object with the same data hash. This suggests a duplicate object exists and has not been "
			"detected, or an object is being added twice, which should not happen",
			m_newSamplers.m_newObjects.find(nameID) == m_newSamplers.m_newObjects.end());

		m_newSamplers.m_newObjects.insert({ nameID, newObject });
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::TextureTargetSet> newObject)
	{
		// Target sets can have identical names, and are usually identified by their unique target configuration (via a
		// data hash). It's possible we might have 2 identical target configurations so we use the uniqueID here instead
		const size_t uniqueID = newObject->GetUniqueID();

		std::lock_guard<std::mutex> lock(m_newTargetSets.m_mutex);

		SEAssert("Found an object with the same data hash. This suggests a duplicate object exists and has not been "
			"detected, or an object is being added twice, which should not happen",
			m_newTargetSets.m_newObjects.find(uniqueID) == m_newTargetSets.m_newObjects.end());

		m_newTargetSets.m_newObjects.insert({ uniqueID, newObject });
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::ParameterBlock> newObject)
	{
		const size_t uniqueID = newObject->GetUniqueID(); // Handle

		std::lock_guard<std::mutex> lock(m_newParameterBlocks.m_mutex);

		SEAssert("Found an object with the same data hash. This suggests a duplicate object exists and has not been "
			"detected, or an object is being added twice, which should not happen",
			m_newParameterBlocks.m_newObjects.find(uniqueID) == m_newParameterBlocks.m_newObjects.end());

		m_newParameterBlocks.m_newObjects.insert({ uniqueID, newObject });
	}


	void RenderManager::CreateAPIResources()
	{
		platform::RenderManager::CreateAPIResources(*this);
	}
}


