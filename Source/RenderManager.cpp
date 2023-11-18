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
#include "ImGuiUtils.h"
#include "PerformanceTimer.h"
#include "Light.h"
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


	constexpr uint8_t RenderManager::GetNumFrames()
	{
		return platform::RenderManager::GetNumFrames();
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
		, m_newShaders(util::NBufferedVector<std::shared_ptr<re::Shader>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newVertexStreams(util::NBufferedVector<std::shared_ptr<re::VertexStream>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newTextures(util::NBufferedVector<std::shared_ptr<re::Texture>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newSamplers(util::NBufferedVector<std::shared_ptr<re::Sampler>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newTargetSets(util::NBufferedVector<std::shared_ptr<re::TextureTargetSet>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newParameterBlocks(util::NBufferedVector<std::shared_ptr<re::ParameterBlock>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_singleFrameVertexStreams(util::NBufferedVector<std::shared_ptr<re::VertexStream>>::BufferSize::Three, k_newObjectReserveAmount)
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

		// Execute each RenderSystem's platform-specific graphics system update pipelines:
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

		// Need to clear the PB read data now, to make sure we're not holding on to any single frame PBs beyond the
		// end of the current frame
		m_newParameterBlocks.ClearReadData();

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

		// Swap the single-frame resource n-buffers:
		m_singleFrameVertexStreams.Swap();

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

		// Clear single-frame resources:
		m_singleFrameVertexStreams.Destroy();

		// Need to do this here so the CoreEngine's Window can be destroyed
		re::Context::Get()->Destroy();

		PIXEndEvent();
	}


	void RenderManager::HandleEvents()
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "re::RenderManager::HandleEvents");

		while (HasEvents())
		{
			en::EventManager::EventInfo const& eventInfo = GetEvent();

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
		m_newVertexStreams.AquireReadLock();
		m_newTextures.AquireReadLock();
		m_newSamplers.AquireReadLock();
		m_newTargetSets.AquireReadLock();
		m_newParameterBlocks.AquireReadLock();

		// Record any newly created textures (we clear m_newTextures during Initialize, so we maintain a separate copy)
		// This allows us an easy way to create MIPs, and clear the initial data after buffering
		for (auto const& newTexture : m_newTextures.GetReadData())
		{
			m_createdTextures.emplace_back(newTexture);
		}

		// Create the resources:
		platform::RenderManager::CreateAPIResources(*this);

		// Release read locks:
		m_newShaders.ReleaseReadLock();
		m_newVertexStreams.ReleaseReadLock();
		m_newTextures.ReleaseReadLock();
		m_newSamplers.ReleaseReadLock();
		m_newTargetSets.ReleaseReadLock();
		m_newParameterBlocks.ReleaseReadLock();

		// Clear the initial data of our new textures now that they have been buffered
		for (auto const& newTexture : m_createdTextures)
		{
			newTexture->ClearTexelData();
		}

		PIXEndEvent();
	}


	void RenderManager::SwapNewResourceDoubleBuffers()
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "RenderManager::SwapNewResourceDoubleBuffers");

		// Swap our new resource double buffers:
		m_newShaders.Swap();
		m_newVertexStreams.Swap();
		m_newTextures.Swap();
		m_newSamplers.Swap();
		m_newTargetSets.Swap();
		m_newParameterBlocks.Swap();

		PIXEndEvent();
	}


	void RenderManager::DestroyNewResourceDoubleBuffers()
	{
		m_newShaders.Destroy();
		m_newVertexStreams.Destroy();
		m_newTextures.Destroy();
		m_newSamplers.Destroy();
		m_newTargetSets.Destroy();
		m_newParameterBlocks.Destroy();
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::Shader> newObject)
	{
		m_newShaders.EmplaceBack(std::move(newObject));
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::VertexStream> newObject)
	{
		m_newVertexStreams.EmplaceBack(std::move(newObject));
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::Texture> newObject)
	{
		m_newTextures.EmplaceBack(std::move(newObject));
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::Sampler> newObject)
	{
		m_newSamplers.EmplaceBack(std::move(newObject));
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::TextureTargetSet> newObject)
	{
		m_newTargetSets.EmplaceBack(std::move(newObject));
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::ParameterBlock> newObject)
	{
		m_newParameterBlocks.EmplaceBack(std::move(newObject));
	}


	template<>
	void RenderManager::RegisterSingleFrameResource(std::shared_ptr<re::VertexStream> singleFrameObject)
	{
		m_singleFrameVertexStreams.EmplaceBack(std::move(singleFrameObject));
	}


	void RenderManager::ShowRenderDebugImGuiWindows(bool* show)
	{
		constexpr char const* renderSystemsPanelTitle = "Render Debug";
		ImGui::Begin(renderSystemsPanelTitle, show);


		if (ImGui::CollapsingHeader("Cameras:", ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();
			std::vector<std::shared_ptr<gr::Camera>> const& cameras = en::SceneManager::GetSceneData()->GetCameras();

			// TODO: Currently, we set the camera parameters as a permanent PB via a shared_ptr from the main camera
			// once in every GS. We need to be able to get/set the main camera's camera params PB every frame, in every
			// GS. Camera selection works, but the GS's all render from the same camera. For now, just disable it.
//#define CAMERA_SELECTION
#if defined(CAMERA_SELECTION)
			static int activeCamIdx = static_cast<int>(m_activeCameraIdx);
			constexpr ImGuiComboFlags k_cameraSelectionflags = 0;
			static int comboSelectedCamIdx = activeCamIdx; // Initialize with the index of the current main camera
			const char* comboPreviewCamName = cameras[comboSelectedCamIdx]->GetName().c_str();
			if (ImGui::BeginCombo("Active camera", comboPreviewCamName, k_cameraSelectionflags))
			{
				for (size_t camIdx = 0; camIdx < cameras.size(); camIdx++)
				{
					const bool isSelected = (comboSelectedCamIdx == camIdx);
					if (ImGui::Selectable(cameras[camIdx]->GetName().c_str(), isSelected))
					{
						comboSelectedCamIdx = static_cast<int>(camIdx);
					}

					if (isSelected) // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();

				// Handle active camera changes:
				if (comboSelectedCamIdx != activeCamIdx)
				{
					activeCamIdx = comboSelectedCamIdx;
					SetMainCameraIdx(comboSelectedCamIdx);
				}
			}
#endif

			for (size_t camIdx = 0; camIdx < cameras.size(); camIdx++)
			{
				cameras[camIdx]->ShowImGuiWindow();
				ImGui::Separator();
			}
			ImGui::Unindent();
		}

		if (ImGui::CollapsingHeader("Meshes:", ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();
			std::vector<std::shared_ptr<gr::Mesh>> const& meshes = en::SceneManager::GetSceneData()->GetMeshes();
			for (auto const& mesh : meshes)
			{
				mesh->ShowImGuiWindow();
				ImGui::Separator();
			}
			ImGui::Unindent();
		}

		ImGui::Separator();

		if (ImGui::CollapsingHeader("Materials:", ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();
			std::unordered_map<size_t, std::shared_ptr<gr::Material>> const& materials = 
				en::SceneManager::GetSceneData()->GetMaterials();
			for (auto const& materialEntry : materials)
			{
				materialEntry.second->ShowImGuiWindow();
				ImGui::Separator();
			}
			ImGui::Unindent();
		}

		ImGui::Separator();

		if (ImGui::CollapsingHeader("Lights:", ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();
			std::shared_ptr<gr::Light> const ambientLight = en::SceneManager::GetSceneData()->GetAmbientLight();
			if (ambientLight)
			{
				ambientLight->ShowImGuiWindow();
			}

			std::shared_ptr<gr::Light> const directionalLight = en::SceneManager::GetSceneData()->GetKeyLight();
			if (directionalLight)
			{
				directionalLight->ShowImGuiWindow();
			}

			if (ImGui::CollapsingHeader("Point Lights:", ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				std::vector<std::shared_ptr<gr::Light>> const& pointLights = 
					en::SceneManager::GetSceneData()->GetPointLights();
				for (auto const& light : pointLights)
				{
					light->ShowImGuiWindow();
				}
				ImGui::Unindent();
			}
			ImGui::Unindent();
		}

		ImGui::Separator();

		// Render systems:
		if (ImGui::CollapsingHeader("Render Systems:", ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();
			for (std::unique_ptr<re::RenderSystem>& renderSystem : m_renderSystems)
			{
				if (ImGui::CollapsingHeader(renderSystem->GetName().c_str(), ImGuiTreeNodeFlags_None))
				{
					ImGui::Indent();
					renderSystem->ShowImGuiWindow();
					ImGui::Unindent();
				}
			}
			ImGui::Unindent();
		}

		ImGui::Separator();
		
		if (ImGui::CollapsingHeader("PIX Captures")) // https://devblogs.microsoft.com/pix/programmatic-capture/
		{
			ImGui::Indent();

			const bool isDX12 = en::Config::Get()->GetRenderingAPI() == platform::RenderingAPI::DX12;
			const bool pixGPUCaptureCmdLineEnabled = isDX12 &&
				en::Config::Get()->ValueExists(en::ConfigKeys::k_pixGPUProgrammaticCapturesCmdLineArg);
			const bool pixCPUCaptureCmdLineEnabled = isDX12 &&
				en::Config::Get()->ValueExists(en::ConfigKeys::k_pixCPUProgrammaticCapturesCmdLineArg);

			if (!pixGPUCaptureCmdLineEnabled && !pixCPUCaptureCmdLineEnabled)
			{
				ImGui::Text("Launch with -pixgpucapture or -pixcpucapture to enable.\n"
					"Run PIX in administrator mode, and attach to the current process.");
			}

			// GPU captures:
			{
				ImGui::BeginDisabled(!pixGPUCaptureCmdLineEnabled);

				static char s_pixGPUCapturePath[256] = "C:\\SaberEngineGPUCapture.wpix";
				static int s_numPixGPUCaptureFrames = 1;
				static HRESULT s_gpuHRESULT = S_OK;
				if (ImGui::Button("Capture PIX GPU Frame"))
				{
					s_gpuHRESULT = PIXGpuCaptureNextFrames(util::ToWideString(s_pixGPUCapturePath).c_str(), s_numPixGPUCaptureFrames);
				}
				ImGui::SetItemTooltip("PIX must be run in administrator mode, and already attached to the process");

				if (s_gpuHRESULT != S_OK)
				{
					const _com_error comError(s_gpuHRESULT);
					const std::string errorMessage = std::format("HRESULT error \"{}\" starting PIX GPU capture.\n"
						"Is PIX running in administrator mode, and attached to the process?",
						comError.ErrorMessage());

					bool showErrorPopup = true;
					util::ShowErrorPopup("Failed to start PIX GPU capture", errorMessage.c_str(), showErrorPopup);
					if (!showErrorPopup)
					{
						s_gpuHRESULT = S_OK;
					}
				}

				ImGui::InputText("Output file", s_pixGPUCapturePath, IM_ARRAYSIZE(s_pixGPUCapturePath));
				ImGui::SetItemTooltip("Must include the .wpix file extension");

				ImGui::SliderInt("No. of frames", &s_numPixGPUCaptureFrames, 1, 10);

				ImGui::EndDisabled();
			}

			ImGui::Separator();

			// CPU timing captures:
			{
				ImGui::BeginDisabled(!pixCPUCaptureCmdLineEnabled);

				static char s_pixCPUCapturePath[256] = "C:\\SaberEngineTimingCapture.wpix";
				static bool s_captureGPUTimings = true;
				static bool s_captureCallstacks = true;
				static bool s_captureCpuSamples = true;
				static const uint32_t s_cpuSamplesPerSecond[3] = {1000u, 4000u, 8000u};
				static int s_cpuSamplesPerSecondSelectionIdx = 0;
				static bool s_captureFileIO = false;
				static bool s_captureVirtualAllocEvents = false;
				static bool s_captureHeapAllocEvents = false;

				static bool s_isCapturing = false;
				static HRESULT s_timingCaptureStartResult = S_OK;
				if (ImGui::Button("Capture PIX CPU Timings"))
				{
					s_isCapturing = true;
					// For compatibility with Xbox, captureFlags must be set to PIX_CAPTURE_GPU or PIX_CAPTURE_TIMING 
					// otherwise the function will return E_NOTIMPL
					const DWORD captureFlags = PIX_CAPTURE_TIMING;

					PIXCaptureParameters pixCaptureParams = PIXCaptureParameters{
						.TimingCaptureParameters{
							.FileName = util::ToWideString(s_pixCPUCapturePath).c_str(),

							.MaximumToolingMemorySizeMb = 0, // Ignored on PIX for Windows
							.CaptureStorage{}, // Ignored on PIX for Windows

							.CaptureGpuTiming = s_captureGPUTimings,

							.CaptureCallstacks = s_captureCallstacks,
							.CaptureCpuSamples = s_captureCpuSamples,
							.CpuSamplesPerSecond = s_cpuSamplesPerSecond[s_cpuSamplesPerSecondSelectionIdx],

							.CaptureFileIO = s_captureFileIO,

							.CaptureVirtualAllocEvents = s_captureVirtualAllocEvents,
							.CaptureHeapAllocEvents = s_captureHeapAllocEvents,
							.CaptureXMemEvents = false, // Xbox only
							.CapturePixMemEvents = false // Xbox only
						}
					};

					s_timingCaptureStartResult = PIXBeginCapture(captureFlags, &pixCaptureParams);
				}

				if (s_timingCaptureStartResult != S_OK)
				{
					const _com_error comError(s_timingCaptureStartResult);
					const std::string errorMessage = std::format("HRESULT error \"{}\" starting PIX timing capture.\n"
						"Is PIX running in administrator mode, and attached to the process?",
						comError.ErrorMessage());

					bool showErrorPopup = true;
					util::ShowErrorPopup("Failed to start PIX timing capture", errorMessage.c_str(), showErrorPopup);
					if (!showErrorPopup)
					{
						s_timingCaptureStartResult = S_OK;
						s_isCapturing = false;
					}
				}

				ImGui::BeginDisabled(!s_isCapturing);
				if (ImGui::Button("End Capture"))
				{
					PIXEndCapture(false);
					s_isCapturing = false;
				}
				ImGui::EndDisabled();

				ImGui::Text("CPU");
				{
					ImGui::Checkbox("CPU samples", &s_captureCpuSamples);

					ImGui::BeginDisabled(!s_captureCpuSamples);
					ImGui::Combo("CPU sampling rate (/sec)", &s_cpuSamplesPerSecondSelectionIdx, 
						"1000\0"
						"4000\0"
						"8000\0\0");
					ImGui::EndDisabled();

					ImGui::Checkbox("Callstacks on context switches", &s_captureCallstacks);
				}
				ImGui::Checkbox("File accesses", &s_captureFileIO);

				ImGui::Checkbox("GPU timings", &s_captureGPUTimings);
				

				ImGui::EndDisabled();
			}

			ImGui::Unindent();
		}
		
		ImGui::End();
	}


	void RenderManager::RenderImGui()
	{
		static bool s_showConsoleLog = false;
		static bool s_showRenderDebug = false;
		static bool s_showImguiDemo = false;

		// Early out if we can:
		if (!m_imguiMenuVisible && !s_showConsoleLog && !s_showRenderDebug && !s_showImguiDemo)
		{
			return;
		}


		platform::RenderManager::StartImGuiFrame();


		const int windowWidth = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowXResValueName);
		const int windowHeight =
			(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowYResValueName));

		// Menu bar:
		ImVec2 menuBarSize = { 0, 0 }; // Record the size of the menu bar so we can align things absolutely underneath it
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
					
					ImGui::MenuItem("Console log", "", &s_showConsoleLog); // Console debug log window

					ImGui::MenuItem("Render Debug", "", &s_showRenderDebug);

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
		if (s_showConsoleLog)
		{
			ImGui::SetNextWindowSize(ImVec2(
				static_cast<float>(windowWidth),
				static_cast<float>(windowHeight * 0.5f)),
				ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowPos(ImVec2(0, menuBarSize[1]), ImGuiCond_FirstUseEver, ImVec2(0, 0));

			en::LogManager::Get()->ShowImGuiWindow(&s_showConsoleLog);
		}

		// Scene objects panel:
		if (s_showRenderDebug)
		{
			ImGui::SetNextWindowSize(ImVec2(
				static_cast<float>(windowWidth) * 0.25f,
				static_cast<float>(windowHeight - menuBarSize[1])),
				ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowPos(ImVec2(0, menuBarSize[1]), ImGuiCond_FirstUseEver, ImVec2(0, 0));

			RenderManager::ShowRenderDebugImGuiWindows(&s_showRenderDebug);
		}

		// Show the ImGui demo window for debugging reference
#if defined(_DEBUG)
		if (s_showImguiDemo)
		{
			ImGui::SetNextWindowPos(
				ImVec2(static_cast<float>(windowWidth) * 0.25f, menuBarSize[1]), ImGuiCond_FirstUseEver, ImVec2(0, 0));
			ImGui::ShowDemoWindow(&s_showImguiDemo);
		}
#endif


		platform::RenderManager::RenderImGui();
	}
}


