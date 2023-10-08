// © 2022 Adam Badke. All rights reserved.
#include <pix3.h>

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
#include "RenderManager_DX12.h"
#include "RenderManager_Platform.h"
#include "RenderManager_OpenGL.h"
#include "SceneManager.h"

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


namespace re
{
	RenderManager* RenderManager::Get()
	{
		static std::unique_ptr<re::RenderManager> instance = std::move(re::RenderManager::Create());
		return instance.get();
	}


	std::unique_ptr<re::RenderManager> RenderManager::Create()
	{
		std::unique_ptr<re::RenderManager> newRenderManager = nullptr;
		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();
		switch (api)
		{
		case platform::RenderingAPI::OpenGL:
		{
			newRenderManager.reset(new opengl::RenderManager());
		}
		break;
		case platform::RenderingAPI::DX12:
		{
			newRenderManager.reset(new dx12::RenderManager());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
		
		return newRenderManager;
	}


	RenderManager::RenderManager()
		: m_renderFrameNum(0)
		, m_imguiMenuVisible(false)
		, m_newShaders(k_newObjectReserveAmount)
		, m_newMeshPrimitives(k_newObjectReserveAmount)
		, m_newTextures(k_newObjectReserveAmount)
		, m_newSamplers(k_newObjectReserveAmount)
		, m_newTargetSets(k_newObjectReserveAmount)
		, m_newParameterBlocks(k_newObjectReserveAmount)
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

			EndOfFrame(); // Clear batches, process pipeline and parameter block allocator EndOfFrames
		}

		// Synchronized shutdown: Blocks main thread until complete
		m_shutdownLatch[static_cast<size_t>(SyncType::ReleaseWorker)].arrive_and_wait();
		Shutdown();
		m_shutdownLatch[static_cast<size_t>(SyncType::ReleaseCommander)].arrive_and_wait();
	}


	void RenderManager::Startup()
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "re::RenderManager::Startup");

		LOG("RenderManager starting...");
		re::Context::Get()->Create();
		en::EventManager::Get()->Subscribe(en::EventManager::InputToggleVSync, this);
		en::EventManager::Get()->Subscribe(en::EventManager::InputToggleConsole, this);

		PIXEndEvent();
	}
	
	
	void RenderManager::Initialize()
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "re::RenderManager::Initialize");

		LOG("RenderManager Initializing...");
		PerformanceTimer timer;
		timer.Start();

		// Build our platform-specific graphics systems:
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "platform::RenderManager::Initialize");
		platform::RenderManager::Initialize(*this);
		PIXEndEvent();

		// Create each render system system in turn:
		for (std::unique_ptr<re::RenderSystem>& renderSystem : m_renderSystems)
		{
			renderSystem->ExecuteCreatePipeline();
		}

		re::Context::Get()->GetParameterBlockAllocator().ClosePermanentPBRegistrationPeriod();

		// Create/buffer new resources from our RenderSystems/GraphicsSystems
		CreateAPIResources();
		ClearNewResourceDoubleBuffers(); // Ensure we don't try and double-create any resources
				
		LOG("\nRenderManager::Initialize complete in %f seconds...\n", timer.StopSec());

		PIXEndEvent();
	}


	void RenderManager::PreUpdate(uint64_t frameNum)
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "re::RenderManager::PreUpdate");

		// Copy frame data:
		SEAssert("Render batches should be empty", m_renderBatches.empty());
		m_renderBatches = std::move(SceneManager::Get()->GetSceneBatches());
		// TODO: Create a BatchManager object that can handle batch double-buffering

		// Execute each RenderSystem's platform-specific update pipelines:
		for (std::unique_ptr<re::RenderSystem>& renderSystem : m_renderSystems)
		{
			renderSystem->ExecuteUpdatePipeline();
		}
	
		// Swap our PB buffers, now that our render systems have written to them
		re::Context::Get()->GetParameterBlockAllocator().SwapBuffers(frameNum);

		// Create any new resources that have been loaded since the last frame:
		CreateAPIResources();

		PIXEndEvent();
	}


	void RenderManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "re::RenderManager::Update");

		HandleEvents();

		// Update/buffer param blocks:
		re::Context::Get()->GetParameterBlockAllocator().BufferParamBlocks();

		// API-specific rendering loop virtual implementations:
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "platform::RenderManager::Render");
		Render();
		PIXEndEvent();

		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "platform::RenderManager::RenderImGui");
		RenderImGui();
		PIXEndEvent();

		// Present the final frame:
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "re::Context::Present");
		re::Context::Get()->Present();
		PIXEndEvent();

		PIXEndEvent();
	}


	void RenderManager::EndOfFrame()
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "re::RenderManager::EndOfFrame");

		// Need to clear the read data now, to make sure we're not holding on to any single frame PBs beyond the end
		// of the current frame
		ClearNewResourceDoubleBuffers();

		m_renderBatches.clear();
		m_createdTextures.clear();

		for (std::unique_ptr<re::RenderSystem>& renderSystem : m_renderSystems)
		{
			re::RenderPipeline& renderPipeline = renderSystem->GetRenderPipeline();
			for (StagePipeline& stagePipeline : renderPipeline.GetStagePipeline())
			{
				stagePipeline.EndOfFrame();
			}
		}

		re::Context::Get()->GetParameterBlockAllocator().EndOfFrame(); 

		PIXEndEvent();
	}


	void RenderManager::Shutdown()
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "re::RenderManager::Shutdown");

		LOG("Render manager shutting down...");
		
		// API-specific destruction:
		platform::RenderManager::Shutdown(*this);

		// NOTE: OpenGL objects must be destroyed on the render thread, so we trigger them here
		en::SceneManager::GetSceneData()->Destroy();
		gr::Material::DestroyMaterialLibrary();
		re::Sampler::DestroySamplerLibrary();
		
		// Destroy render systems:
		for (std::unique_ptr<re::RenderSystem>& renderSystem : m_renderSystems)
		{
			renderSystem->Destroy();
		}

		m_createdTextures.clear();
		m_renderBatches.clear();

		// Clear the new object queues:
		DestroyNewResourceDoubleBuffers();

		// Need to do this here so the CoreEngine's Window can be destroyed
		re::Context::Get()->Destroy();

		PIXEndEvent();
	}


	void RenderManager::HandleEvents()
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "re::RenderManager::HandleEvents");

		while (HasEvents())
		{
			en::EventManager::EventInfo eventInfo = GetEvent();

			switch (eventInfo.m_type)
			{
			case en::EventManager::EventType::InputToggleVSync:
			{
				if (eventInfo.m_data0.m_dataB == true)
				{
					m_vsyncEnabled = !m_vsyncEnabled;
					re::Context::Get()->GetSwapChain().SetVSyncMode(m_vsyncEnabled);
					LOG("VSync %s", m_vsyncEnabled ? "enabled" : "disabled");
				}				
			}
			break;
			case en::EventManager::EventType::InputToggleConsole:
			{
				if (eventInfo.m_data0.m_dataB == true)
				{
					m_imguiMenuVisible = !m_imguiMenuVisible;
				}
			}
			break;
			default:
			{
				SEAssertF("Unexpected event type received");
			}
			break;
			}
		}

		PIXEndEvent();
	}


	void RenderManager::CreateAPIResources()
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "platform::RenderManager::CreateAPIResources");

		// Make our write buffer the new read buffer:
		SwapNewResourceDoubleBuffers();

		// Aquire read locks:
		m_newShaders.AquireReadLock();
		m_newMeshPrimitives.AquireReadLock();
		m_newTextures.AquireReadLock();
		m_newSamplers.AquireReadLock();
		m_newTargetSets.AquireReadLock();
		m_newParameterBlocks.AquireReadLock();

		// Record any newly created textures (we clear m_newTextures during Initialize, so we maintain a separate copy):
		for (auto const& newTexture : m_newTextures.Get())
		{
			m_createdTextures.emplace_back(newTexture.second);
		}

		// Create the resources:
		platform::RenderManager::CreateAPIResources(*this);

		// Release read locks:
		m_newShaders.ReleaseReadLock();
		m_newMeshPrimitives.ReleaseReadLock();
		m_newTextures.ReleaseReadLock();
		m_newSamplers.ReleaseReadLock();
		m_newTargetSets.ReleaseReadLock();
		m_newParameterBlocks.ReleaseReadLock();

		PIXEndEvent();
	}


	void RenderManager::SwapNewResourceDoubleBuffers()
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "RenderManager::SwapNewResourceDoubleBuffers");

		// Swap our new resource double buffers:
		m_newShaders.Swap();
		m_newMeshPrimitives.Swap();
		m_newTextures.Swap();
		m_newSamplers.Swap();
		m_newTargetSets.Swap();
		m_newParameterBlocks.Swap();

		PIXEndEvent();
	}


	void RenderManager::ClearNewResourceDoubleBuffers()
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "RenderManager::ClearNewResourceDoubleBuffers");

		m_newShaders.EndOfFrame();
		m_newMeshPrimitives.EndOfFrame();
		m_newTextures.EndOfFrame();
		m_newSamplers.EndOfFrame();
		m_newTargetSets.EndOfFrame();
		m_newParameterBlocks.EndOfFrame();

		PIXEndEvent();
	}


	void RenderManager::DestroyNewResourceDoubleBuffers()
	{
		m_newShaders.Destroy();
		m_newMeshPrimitives.Destroy();
		m_newTextures.Destroy();
		m_newSamplers.Destroy();
		m_newTargetSets.Destroy();
		m_newParameterBlocks.Destroy();
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::Shader> newObject)
	{
		const size_t nameID = newObject->GetNameID(); // Shaders are required to have unique names
		m_newShaders.Set(nameID, std::move(newObject));
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::MeshPrimitive> newObject)
	{
		const size_t dataHash = newObject->GetDataHash(); // MeshPrimitives can have duplicate names
		m_newMeshPrimitives.Set(dataHash, std::move(newObject));
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::Texture> newObject)
	{
		const size_t nameID = newObject->GetNameID(); // Textures are required to have unique names
		m_newTextures.Set(nameID, std::move(newObject));
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::Sampler> newObject)
	{
		// Samplers are (currently) required to have unique names (e.g. "Wrap_Linear_Linear")
		const size_t nameID = newObject->GetNameID();
		m_newSamplers.Set(nameID, std::move(newObject));
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::TextureTargetSet> newObject)
	{
		// Target sets can have identical names, and are usually identified by their unique target configuration (via a
		// data hash). It's possible we might have 2 identical target configurations so we use the uniqueID here instead
		const size_t uniqueID = newObject->GetUniqueID();
		m_newTargetSets.Set(uniqueID, std::move(newObject));
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::ParameterBlock> newObject)
	{
		const size_t uniqueID = newObject->GetUniqueID(); // Handle
		m_newParameterBlocks.Set(uniqueID, std::move(newObject));
	}


	void RenderManager::ShowRenderSystemImGuiWindows(bool* show)
	{
		constexpr char const* renderSystemsPanelTitle = "Render Systems";
		ImGui::Begin(renderSystemsPanelTitle, show);
		for (std::unique_ptr<re::RenderSystem>& renderSystem : m_renderSystems)
		{
			if (ImGui::TreeNode(renderSystem->GetName().c_str()))
			{
				renderSystem->ShowImGuiWindow();
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}


	void RenderManager::RenderImGui()
	{
		static bool s_showConsoleLog = false;
		static bool s_showScenePanel = false;
		static bool s_showGraphicsSystemPanel = false;
		static bool s_showImguiDemo = false;

		// Early out if we can:
		if (!m_imguiMenuVisible && !s_showConsoleLog && !s_showScenePanel && !s_showImguiDemo)
		{
			return;
		}


		platform::RenderManager::StartImGuiFrame();


		const int windowWidth = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowXResValueName);
		const int windowHeight =
			(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowYResValueName));

		// Menu bar:
		ImVec2 menuBarSize = { 0, 0 }; // Record the size of the menu bar so we can align things absolutely underneath it
		uint32_t menuDepth = 0; // Ensure windows don't overlap when they're first opened
		if (m_imguiMenuVisible)
		{
			ImGui::BeginMainMenuBar();
			{
				menuBarSize = ImGui::GetWindowSize();

				if (ImGui::BeginMenu("Scene"))
				{
					// TODO...
					ImGui::TextDisabled("Load Scene");
					ImGui::TextDisabled("Reload Scene");
					ImGui::TextDisabled("Reload Shaders");
					ImGui::TextDisabled("Reload Materials");
					ImGui::TextDisabled("Quit");

					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Window"))
				{
					// Console debug log window:
					ImGui::MenuItem("Console log", "", &s_showConsoleLog);

					// Scene objects window:
					ImGui::MenuItem("Scene Objects Panel", "", &s_showScenePanel);

					// Graphics systems window:
					ImGui::MenuItem("Render Systems Panel", "", &s_showGraphicsSystemPanel);

					ImGui::TextDisabled("Performance statistics");


#if defined(_DEBUG) // ImGui demo window
					ImGui::Separator();
					ImGui::MenuItem("Show ImGui demo", "", &s_showImguiDemo);
#endif

					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Display"))
				{
					// TODO...
					ImGui::TextDisabled("Wireframe mode");
					ImGui::TextDisabled("Show bounding boxes");
					ImGui::TextDisabled("View texture/buffer");

					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Config"))
				{
					// TODO...
					ImGui::TextDisabled("Adjust input settings");

					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Output"))
				{
					// TODO...
					ImGui::TextDisabled("Save screenshot");

					ImGui::EndMenu();
				}
			}
			ImGui::EndMainMenuBar();
		}

		// Console log window:
		menuDepth++;
		if (s_showConsoleLog)
		{
			ImGui::SetNextWindowSize(ImVec2(
				static_cast<float>(windowWidth),
				static_cast<float>(windowHeight * 0.5f)),
				ImGuiCond_Once);
			ImGui::SetNextWindowPos(ImVec2(0, menuBarSize[1] * menuDepth), ImGuiCond_Once, ImVec2(0, 0));

			en::LogManager::Get()->ShowImGuiWindow(&s_showConsoleLog);
		}

		// Scene objects panel:
		menuDepth++;
		if (s_showScenePanel)
		{
			ImGui::SetNextWindowSize(ImVec2(
				static_cast<float>(windowWidth) * 0.25f,
				static_cast<float>(windowHeight * 0.75f)),
				ImGuiCond_Once);
			ImGui::SetNextWindowPos(ImVec2(0, menuBarSize[1] * menuDepth), ImGuiCond_Once, ImVec2(0, 0));

			en::SceneManager::Get()->ShowImGuiWindow(&s_showScenePanel);
		}

		// Graphics Systems panel:
		menuDepth++;
		if (s_showGraphicsSystemPanel)
		{
			ImGui::SetNextWindowSize(ImVec2(
				static_cast<float>(windowWidth) * 0.25f,
				static_cast<float>(windowHeight * 0.75f)),
				ImGuiCond_Once);
			ImGui::SetNextWindowPos(ImVec2(0, menuBarSize[1] * menuDepth), ImGuiCond_Once, ImVec2(0, 0));

			RenderManager::ShowRenderSystemImGuiWindows(&s_showGraphicsSystemPanel);
		}

		// Show the ImGui demo window for debugging reference
#if defined(_DEBUG)
		menuDepth++;
		if (s_showImguiDemo)
		{
			ImGui::SetNextWindowPos(ImVec2(0, menuBarSize[1] * menuDepth), ImGuiCond_Once, ImVec2(0, 0));
			ImGui::ShowDemoWindow(&s_showImguiDemo);
		}
#endif


		platform::RenderManager::RenderImGui();
	}
}


