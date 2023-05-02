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


	RenderManager* RenderManager::Get()
	{
		static std::unique_ptr<re::RenderManager> instance = std::make_unique<re::RenderManager>();
		return instance.get();
	}


	RenderManager::RenderManager()
		: m_pipeline("Main pipeline")
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


		EngineThread::ThreadUpdateParams updateParams;

		m_isRunning = true;
		while (m_isRunning)
		{
			// Blocks until a new update params is received, or the EngineThread has been signaled to stop
			const bool doUpdate = GetUpdateParams(updateParams);
			if (!doUpdate)
			{
				break;
			}

			// Copy stage: Blocks other threads until complete
			PreUpdate(updateParams.m_frameNum);
			const std::barrier<>::arrival_token& copyArrive = copyBarrier->arrive();

			Update(updateParams.m_frameNum, updateParams.m_elapsed);
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
		{
			vector<shared_ptr<GraphicsSystem>>::iterator gsIt;
			for (gsIt = m_graphicsSystems.begin(); gsIt != m_graphicsSystems.end(); gsIt++)
			{
				(*gsIt)->Create(m_pipeline.AddNewStagePipeline((*gsIt)->GetName()));

				// Remove GS if it didn't attach any render stages (Ensuring indexes of m_pipeline & m_graphicsSystems match)
				if (m_pipeline.GetPipeline().back().GetNumberOfStages() == 0)
				{
					m_pipeline.GetPipeline().pop_back();
					vector<shared_ptr<GraphicsSystem>>::iterator deleteIt = gsIt;
					
					// Handle the case where the 1st GS didn't append anything
					const bool isFirstGS = gsIt == m_graphicsSystems.begin();
					if (isFirstGS == false)
					{
						gsIt--;		
					}

					m_graphicsSystems.erase(deleteIt);

					// Iterators at/after the erase point are invalidated: Reset our iterator to the start
					if (isFirstGS)
					{
						if (m_graphicsSystems.empty())
						{
							break;
						}
						gsIt = m_graphicsSystems.begin();
					}
				}
			}
		}

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
			m_graphicsSystems[gs]->PreRender(m_pipeline.GetPipeline()[gs]);
		}

		m_context.GetParameterBlockAllocator().SwapBuffers(frameNum);
	}


	void RenderManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		HandleEvents();

		// Update/buffer param blocks:
		m_context.GetParameterBlockAllocator().BufferParamBlocks();

		// Create any new resources that have been loaded since the last frame:
		CreateAPIResources();

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

		for (StagePipeline& stagePipeline : m_pipeline.GetPipeline())
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

		m_pipeline.Destroy();
		m_graphicsSystems.clear();

		m_newShaders.clear();

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


	void RenderManager::RegisterShaderForCreate(std::shared_ptr<re::Shader> newShader)
	{
		const size_t newShaderNameID = newShader->GetNameID();

		std::lock_guard<std::mutex> lock(m_newShadersMutex);

		SEAssert("Found a shader with the same name, but a different raw pointer. This suggests a duplicate Shader, "
			"which should not be possible", 
			m_newShaders.find(newShaderNameID) == m_newShaders.end() || 
				m_newShaders.at(newShaderNameID).get() == newShader.get());

		m_newShaders.insert({ newShader->GetNameID(), newShader });
	}


	void RenderManager::CreateAPIResources()
	{
		platform::RenderManager::CreateAPIResources(*this);
	}
}


