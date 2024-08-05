// © 2022 Adam Badke. All rights reserved.
#include "Context.h"
#include "GraphicsSystemManager.h"
#include "RenderManager.h"
#include "RenderManager_DX12.h"
#include "RenderManager_Platform.h"
#include "RenderManager_OpenGL.h"
#include "Sampler.h"
#include "TextureTarget.h"
#include "VertexStream.h"

#include "Core/Config.h"
#include "Core/PerformanceTimer.h"
#include "Core/ProfilingMarkers.h"


namespace re
{
	RenderManager* RenderManager::Get()
	{
		static std::unique_ptr<re::RenderManager> instance = std::move(re::RenderManager::Create());
		return instance.get();
	}


	uint8_t RenderManager::GetNumFramesInFlight()
	{
		return platform::RenderManager::GetNumFramesInFlight();
	}


	float RenderManager::GetWindowAspectRatio()
	{
		core::Config const* config = core::Config::Get();
		const int width = config->GetValue<int>(core::configkeys::k_windowWidthKey);
		const int height = config->GetValue<int>(core::configkeys::k_windowHeightKey);
		return static_cast<float>(width) / height;
	}


	std::unique_ptr<re::RenderManager> RenderManager::Create()
	{
		core::Config* config = core::Config::Get();

		platform::RenderingAPI renderingAPI = platform::RenderingAPI::RenderingAPI_Count;
		if (config->KeyExists(core::configkeys::k_platformCmdLineArg))
		{
			std::string const& platformParam = config->GetValue<std::string>(core::configkeys::k_platformCmdLineArg);

			if (platformParam.find("opengl") != std::string::npos)
			{
				renderingAPI = platform::RenderingAPI::OpenGL;
			}
			else if (platformParam.find("dx12") != std::string::npos)
			{
				renderingAPI = platform::RenderingAPI::DX12;
			}
		}
		else
		{
			renderingAPI = platform::RenderingAPI::DX12; // Default when no "-platform <API>" override received
		}

		std::unique_ptr<re::RenderManager> newRenderManager = nullptr;

		switch (renderingAPI)
		{
		case platform::RenderingAPI::DX12:
		{
			config->TrySetValue(
				core::configkeys::k_shaderDirectoryKey,
				std::string(core::configkeys::k_hlslShaderDirName),
				core::Config::SettingType::Runtime);

			config->TrySetValue(
				core::configkeys::k_numBackbuffersKey,
				3,
				core::Config::SettingType::Runtime);

			newRenderManager.reset(new dx12::RenderManager());
		}
		break;
		case platform::RenderingAPI::OpenGL:
		{
			config->TrySetValue(
				core::configkeys::k_shaderDirectoryKey,
				std::string(core::configkeys::k_glslShaderDirName),
				core::Config::SettingType::Runtime);

			config->TrySetValue(
				core::configkeys::k_numBackbuffersKey,
				2, // Note: OpenGL only supports double-buffering
				core::Config::SettingType::Runtime);

			newRenderManager.reset(new opengl::RenderManager());
		}
		break;
		default: SEAssertF("Invalid rendering API value");
		}

		newRenderManager->m_sceneData = std::make_unique<re::SceneData>();
		
		return newRenderManager;
	}


	RenderManager::RenderManager(platform::RenderingAPI renderingAPI)
		: m_renderingAPI(renderingAPI)
		, m_sceneData(nullptr)
		, m_renderFrameNum(0)
		, m_renderCommandManager(k_renderCommandBufferSize)
		, m_newShaders(util::NBufferedVector<std::shared_ptr<re::Shader>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newVertexStreams(util::NBufferedVector<std::shared_ptr<re::VertexStream>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newTextures(util::NBufferedVector<std::shared_ptr<re::Texture>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newSamplers(util::NBufferedVector<std::shared_ptr<re::Sampler>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newTargetSets(util::NBufferedVector<std::shared_ptr<re::TextureTargetSet>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newBuffers(util::NBufferedVector<std::shared_ptr<re::Buffer>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_singleFrameVertexStreams(util::NBufferedVector<std::shared_ptr<re::VertexStream>>::BufferSize::Three, k_newObjectReserveAmount)
		, m_quitEventRecieved(false)
	{
		m_vsyncEnabled = core::Config::Get()->GetValue<bool>(core::configkeys::k_vsyncEnabledKey);
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


		IEngineThread::ThreadUpdateParams updateParams{};

		m_isRunning = true;
		while (m_isRunning)
		{
			SEBeginCPUEvent("RenderManager frame");

			// Blocks until a new update params is received, or the IEngineThread has been signaled to stop
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

			EndOfFrame(); // Clear batches, process pipeline and buffer allocator EndOfFrames

			SEEndCPUEvent();
		}

		// Synchronized shutdown: Blocks main thread until complete
		m_shutdownLatch[static_cast<size_t>(SyncType::ReleaseWorker)].arrive_and_wait();
		Shutdown();
		m_shutdownLatch[static_cast<size_t>(SyncType::ReleaseCommander)].arrive_and_wait();
	}


	void RenderManager::Startup()
	{
		SEBeginCPUEvent("re::RenderManager::Startup");

		LOG("RenderManager starting...");
		re::Context::Get()->Create(m_renderFrameNum);
		core::EventManager::Get()->Subscribe(core::EventManager::InputToggleVSync, this);
		core::EventManager::Get()->Subscribe(core::EventManager::EngineQuit, this);

		SEEndCPUEvent();
	}
	
	
	void RenderManager::Initialize()
	{
		SEBeginCPUEvent("re::RenderManager::Initialize");

		LOG("RenderManager Initializing...");
		util::PerformanceTimer timer;
		timer.Start();

		m_effectDB.LoadEffectManifest();

		m_renderData.BeginFrame(m_renderFrameNum);
		m_lightManager.Initialize();

		SEBeginCPUEvent("platform::RenderManager::Initialize");
		platform::RenderManager::Initialize(*this);
		SEEndCPUEvent();

		// Process any pending render commands to ensure we've processed anything queued during creation
		m_renderCommandManager.SwapBuffers();
		m_renderCommandManager.Execute();

		// Create/buffer new resources added by our RenderSystems/GraphicsSystems. During Initialize(), most data has
		// just been loaded so there is typically something of all types to create here
		CreateAPIResources();
				
		LOG("\nRenderManager::Initialize complete in %f seconds...\n", timer.StopSec());

		SEEndCPUEvent();
	}


	re::RenderSystem const* RenderManager::CreateAddRenderSystem(
		std::string const& name, std::string const& pipelineFileName)
	{
		m_renderSystems.emplace_back(re::RenderSystem::Create(name, pipelineFileName));

		// Initialize the render system (which will in turn initialize each of its graphics systems & stage pipelines)
		m_renderSystems.back()->ExecuteInitializationPipeline();

		return m_renderSystems.back().get();
	}


	void RenderManager::PreUpdate(uint64_t frameNum)
	{
		// This function is blocking; We do the bare minimum work possible here
		SEBeginCPUEvent("re::RenderManager::PreUpdate");
		
		m_renderCommandManager.SwapBuffers();

		SEEndCPUEvent();
	}


	void RenderManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		SEBeginCPUEvent("re::RenderManager::Update");

		HandleEvents();
		if (m_quitEventRecieved)
		{
			return; // Early-out: Prevents issues related to queued ImGui commands referring to now-destroyed data
		}

		re::Context::Get()->GetBufferAllocator()->BeginFrame(frameNum);
		
		// Get the RenderDataManager ready for the new frame
		m_renderData.BeginFrame(m_renderFrameNum);

		m_renderCommandManager.Execute(); // Process render commands. Must happen 1st to ensure RenderData is up to date

		// Update the batch manager now that the RenderData has been populated
		m_lightManager.UpdateLightBuffers(m_renderData);
		m_batchManager.UpdateBatchCache(m_renderData);

		// Execute each RenderSystem's platform-specific graphics system update pipelines:
		SEBeginCPUEvent("Execute update pipeline");
		for (std::unique_ptr<re::RenderSystem>& renderSystem : m_renderSystems)
		{
			renderSystem->ExecuteUpdatePipeline();
			renderSystem->PostUpdatePreRender();
		}
		SEEndCPUEvent();

		// Create any new resources that have been created by GS's during the ExecuteUpdatePipeline call:
		CreateAPIResources();

		// Update/buffer param blocks
		re::Context::Get()->GetBufferAllocator()->BufferData();

		// API-specific rendering loop virtual implementations:
		SEBeginCPUEvent("platform::RenderManager::Render");
		Render();
		SEEndCPUEvent();

		// Present the finished frame:
		SEBeginCPUEvent("re::Context::Present");
		re::Context::Get()->Present();
		SEEndCPUEvent();

		SEEndCPUEvent();
	}


	void RenderManager::EndOfFrame()
	{
		SEBeginCPUEvent("re::RenderManager::EndOfFrame");

		// Need to clear the buffer read data now, to make sure we're not holding on to any single frame buffers beyond the
		// end of the current frame
		SEBeginCPUEvent("Clear buffer data");
		{
			m_newBuffers.ClearReadData();
		}

		SEEndCPUEvent();

		SEBeginCPUEvent("Process render systems");
		{
			for (std::unique_ptr<re::RenderSystem>& renderSystem : m_renderSystems)
			{
				renderSystem->EndOfFrame();
			}
		}
		SEEndCPUEvent();

		SEBeginCPUEvent("Swap buffers");
		{
			// Swap the single-frame resource n-buffers:
			m_singleFrameVertexStreams.Swap();

			re::Context::Get()->GetBufferAllocator()->EndFrame();
		}
		SEEndCPUEvent();

		ProcessDeferredDeletions(GetCurrentRenderFrameNum());

		SEEndCPUEvent();
	}


	void RenderManager::RegisterTextureForDeferredDelete(std::unique_ptr<re::Texture::PlatformParams>&& platParams)
	{
		{
			std::lock_guard<std::mutex> lock(m_deletedTexturesMutex);

			m_deletedTextures.emplace(TextureDeferredDelete{
				.m_platformParams = std::move(platParams),
				.m_frameNum = GetCurrentRenderFrameNum() });
		}
	}


	void RenderManager::ProcessDeferredDeletions(uint64_t frameNum)
	{
		const uint8_t numFramesInFlight = GetNumFramesInFlight();
		{
			std::lock_guard<std::mutex> lock(m_deletedTexturesMutex);

			while (!m_deletedTextures.empty() &&
				m_deletedTextures.front().m_frameNum + numFramesInFlight < frameNum)
			{
				m_deletedTextures.front().m_platformParams->Destroy();
				m_deletedTextures.pop();
			}
		}
	}


	void RenderManager::Shutdown()
	{
		SEBeginCPUEvent("re::RenderManager::Shutdown");

		LOG("Render manager shutting down...");

		// Flush any remaining render work:
		platform::RenderManager::Shutdown(*this);

		// Process any remaining render commands (i.e. delete platform objects)
		m_renderCommandManager.SwapBuffers();
		m_renderCommandManager.Execute();

		m_sceneData->Destroy();

		m_effectDB.Destroy();

		re::Sampler::DestroySamplerLibrary();
		
		// Destroy render systems:
		for (std::unique_ptr<re::RenderSystem>& renderSystem : m_renderSystems)
		{
			renderSystem->Destroy();
		}

		m_renderData.Destroy();
		m_lightManager.Destroy();

		m_createdTextures.clear();

		// Clear the new object queues:
		DestroyNewResourceDoubleBuffers();

		// Clear single-frame resources:
		m_singleFrameVertexStreams.Destroy();

		// Destroy the swap chain before forcing deferred deletions. This is safe, as we've already flushed any
		// remaining outstanding work
		re::Context::Get()->GetSwapChain().Destroy();

		ProcessDeferredDeletions(k_forceDeferredDeletionsFlag); // Force-delete everything

		// Need to do this here so the EngineApp's Window can be destroyed
		re::Context::Get()->Destroy();

		SEEndCPUEvent();
	}


	void RenderManager::HandleEvents()
	{
		SEBeginCPUEvent("re::RenderManager::HandleEvents");

		while (HasEvents())
		{
			core::EventManager::EventInfo const& eventInfo = GetEvent();

			switch (eventInfo.m_type)
			{
			case core::EventManager::InputToggleVSync:
			{
				if (eventInfo.m_data0.m_dataB == true)
				{
					m_vsyncEnabled = !m_vsyncEnabled;
					re::Context::Get()->GetSwapChain().SetVSyncMode(m_vsyncEnabled);
					LOG("VSync %s", m_vsyncEnabled ? "enabled" : "disabled");
				}				
			}
			break;
			case core::EventManager::EngineQuit:
			{
				m_quitEventRecieved = true;
			}
			break;
			default:
			{
				SEAssertF("Unexpected event type received");
			}
			break;
			}
		}

		SEEndCPUEvent();
	}


	void RenderManager::CreateAPIResources()
	{
		SEBeginCPUEvent("platform::RenderManager::CreateAPIResources");

		// Make our write buffer the new read buffer:
		SwapNewResourceDoubleBuffers();

		// Aquire read locks:
		m_newShaders.AquireReadLock();
		m_newVertexStreams.AquireReadLock();
		m_newTextures.AquireReadLock();
		m_newSamplers.AquireReadLock();
		m_newTargetSets.AquireReadLock();
		m_newBuffers.AquireReadLock();

		// Clear any textures created during the last frame as late as possible. This allows systems that use this list
		// (e.g. the MIP generation GS) to see new textures registered for creation between the last time we swapped
		// the m_newTextures buffers and now
		m_createdTextures.clear();

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
		m_newBuffers.ReleaseReadLock();

		// Clear the initial data of our new textures now that they have been buffered
		for (auto const& newTexture : m_createdTextures)
		{
			newTexture->ClearTexelData();
		}

		SEEndCPUEvent();
	}


	void RenderManager::SwapNewResourceDoubleBuffers()
	{
		SEBeginCPUEvent("RenderManager::SwapNewResourceDoubleBuffers");

		// Swap our new resource double buffers:
		m_newShaders.Swap();
		m_newVertexStreams.Swap();
		m_newTextures.Swap();
		m_newSamplers.Swap();
		m_newTargetSets.Swap();
		m_newBuffers.Swap();

		SEEndCPUEvent();
	}


	void RenderManager::DestroyNewResourceDoubleBuffers()
	{
		m_newShaders.Destroy();
		m_newVertexStreams.Destroy();
		m_newTextures.Destroy();
		m_newSamplers.Destroy();
		m_newTargetSets.Destroy();
		m_newBuffers.Destroy();
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
	void RenderManager::RegisterForCreate(std::shared_ptr<re::Buffer> newObject)
	{
		m_newBuffers.EmplaceBack(std::move(newObject));
	}


	template<>
	void RenderManager::RegisterSingleFrameResource(std::shared_ptr<re::VertexStream> singleFrameObject)
	{
		m_singleFrameVertexStreams.EmplaceBack(std::move(singleFrameObject));
	}


	void RenderManager::ShowRenderSystemsImGuiWindow(bool* show)
	{
		if (!(*show))
		{
			return;
		}

		ImGui::Begin(std::format("Render Systems ({})", m_renderSystems.size()).c_str(), show);

		// Render systems:
		for (std::unique_ptr<re::RenderSystem>& renderSystem : m_renderSystems)
		{
			if (ImGui::CollapsingHeader(renderSystem->GetName().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Indent();
				renderSystem->ShowImGuiWindow();
				ImGui::Unindent();
			}
		}

		ImGui::End();
	}


	void RenderManager::ShowGPUCapturesImGuiWindow(bool* show)
	{
		if (!(*show))
		{
			return;
		}

		ImGui::Begin("GPU Captures", show);

		if (ImGui::CollapsingHeader("RenderDoc"))
		{
			ImGui::Indent();

			re::Context::RenderDocAPI* renderDocApi = re::Context::Get()->GetRenderDocAPI();

			const bool renderDocCmdLineEnabled =
				core::Config::Get()->KeyExists(core::configkeys::k_renderDocProgrammaticCapturesCmdLineArg) &&
				renderDocApi != nullptr;

			if (!renderDocCmdLineEnabled)
			{
				ImGui::Text(std::format("Launch with -{} to enable",
					core::configkeys::k_renderDocProgrammaticCapturesCmdLineArg).c_str());
			}
			else
			{
				int major, minor, patch;
				renderDocApi->GetAPIVersion(&major, &minor, &patch);
				ImGui::Text(std::format("Renderdoc API {}.{}.{}", major, minor, patch).c_str());

				if (ImGui::CollapsingHeader("View capture options"))
				{
					ImGui::Indent();

					ImGui::Text(std::format("Allow VSync: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_AllowVSync)).c_str());

					ImGui::Text(std::format("Allow fullscreen: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_AllowFullscreen)).c_str());

					ImGui::Text(std::format("API validation: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_APIValidation)).c_str());

					ImGui::Text(std::format("Capture callstacks: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_CaptureCallstacks)).c_str());

					ImGui::Text(std::format("Only capture callstacks for actions: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_CaptureCallstacksOnlyActions)).c_str());

					ImGui::Text(std::format("Debugger attach delay: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_DelayForDebugger)).c_str());

					ImGui::Text(std::format("Verify buffer access: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_VerifyBufferAccess)).c_str());

					ImGui::Text(std::format("Hook into child processes: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_HookIntoChildren)).c_str());

					ImGui::Text(std::format("Reference all resources: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_RefAllResources)).c_str());

					ImGui::Text(std::format("Capture all command lists from start: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_CaptureAllCmdLists)).c_str());

					ImGui::Text(std::format("Mute API debugging output: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_DebugOutputMute)).c_str());

					ImGui::Text(std::format("Allow unsupported vendor extensions: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_AllowUnsupportedVendorExtensions)).c_str());

					ImGui::Text(std::format("Soft memory limit: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_SoftMemoryLimit)).c_str());

					ImGui::Unindent();
				}
				if (ImGui::CollapsingHeader("Configure overlay"))
				{
					ImGui::Indent();
					const uint32_t overlayBits = renderDocApi->GetOverlayBits();

					static bool s_overlayEnabled = (overlayBits & RENDERDOC_OverlayBits::eRENDERDOC_Overlay_Enabled);
					ImGui::Checkbox("Display overlay?", &s_overlayEnabled);

					static bool s_overlayFramerate = (overlayBits & RENDERDOC_OverlayBits::eRENDERDOC_Overlay_FrameRate);
					ImGui::Checkbox("Frame rate", &s_overlayFramerate);

					static bool s_overlayFrameNum = (overlayBits & RENDERDOC_OverlayBits::eRENDERDOC_Overlay_FrameNumber);
					ImGui::Checkbox("Frame number", &s_overlayFrameNum);

					static bool s_overlayCaptureList = (overlayBits & RENDERDOC_OverlayBits::eRENDERDOC_Overlay_CaptureList);
					ImGui::Checkbox("Recent captures", &s_overlayCaptureList);

					renderDocApi->MaskOverlayBits(
						0,
						(s_overlayEnabled ? eRENDERDOC_Overlay_Enabled : 0) |
						(s_overlayFramerate ? eRENDERDOC_Overlay_FrameRate : 0) |
						(s_overlayFrameNum ? eRENDERDOC_Overlay_FrameNumber : 0) |
						(s_overlayCaptureList ? eRENDERDOC_Overlay_CaptureList : 0));

					ImGui::Unindent();
				}

				static char s_renderDocCaptureFolder[256];
				static bool s_loadedPath = false;
				if (!s_loadedPath)
				{
					s_loadedPath = true;
					memcpy(s_renderDocCaptureFolder,
						renderDocApi->GetCaptureFilePathTemplate(),
						strlen(renderDocApi->GetCaptureFilePathTemplate()) + 1);
				}

				if (ImGui::InputText("Output path & prefix", s_renderDocCaptureFolder, IM_ARRAYSIZE(s_renderDocCaptureFolder)))
				{
					renderDocApi->SetCaptureFilePathTemplate(s_renderDocCaptureFolder);
				}

				static int s_numRenderDocFrames = 1;
				if (ImGui::Button("Capture RenderDoc Frame"))
				{
					renderDocApi->TriggerMultiFrameCapture(s_numRenderDocFrames);
				}
				ImGui::SliderInt("No. of frames", &s_numRenderDocFrames, 1, 10);
			}
			{
				ImGui::BeginDisabled(!renderDocCmdLineEnabled);

				ImGui::EndDisabled();
			}
			ImGui::Unindent();
		}

		ImGui::Separator();

		if (ImGui::CollapsingHeader("PIX Captures")) // https://devblogs.microsoft.com/pix/programmatic-capture/
		{
			ImGui::Indent();

			const bool isDX12 = m_renderingAPI == platform::RenderingAPI::DX12;
			const bool pixGPUCaptureCmdLineEnabled = isDX12 &&
				core::Config::Get()->KeyExists(core::configkeys::k_pixGPUProgrammaticCapturesCmdLineArg);
			const bool pixCPUCaptureCmdLineEnabled = isDX12 &&
				core::Config::Get()->KeyExists(core::configkeys::k_pixCPUProgrammaticCapturesCmdLineArg);

			if (!pixGPUCaptureCmdLineEnabled && !pixCPUCaptureCmdLineEnabled)
			{
				ImGui::Text(std::format("Launch with -{} or -{} to enable.\n"
					"Run PIX in administrator mode, and attach to the current process.",
					core::configkeys::k_pixGPUProgrammaticCapturesCmdLineArg,
					core::configkeys::k_pixCPUProgrammaticCapturesCmdLineArg).c_str());
			}

			// GPU captures:
			if (pixGPUCaptureCmdLineEnabled)
			{
				ImGui::BeginDisabled(!pixGPUCaptureCmdLineEnabled);

				static char s_pixGPUCapturePath[256];
				static bool loadedPath = false;
				if (!loadedPath)
				{
					loadedPath = true;
					std::string const& pixFilePath = std::format("{}\\{}\\",
						core::Config::Get()->GetValueAsString(core::configkeys::k_documentsFolderPathKey),
						core::configkeys::k_pixCaptureFolderName);
					memcpy(s_pixGPUCapturePath, pixFilePath.c_str(), pixFilePath.length() + 1);
				}

				static int s_numPixGPUCaptureFrames = 1;
				static HRESULT s_gpuHRESULT = S_OK;
				if (ImGui::Button("Capture PIX GPU Frame"))
				{
					std::wstring const& filepath = util::ToWideString(
						std::format("{}\\{}GPUCapture_{}.wpix",
							s_pixGPUCapturePath,
							core::configkeys::k_captureTitle,
							util::GetTimeAndDateAsString()));
					s_gpuHRESULT = PIXGpuCaptureNextFrames(filepath.c_str(), s_numPixGPUCaptureFrames);
				}
				ImGui::SetItemTooltip("PIX must be run in administrator mode, and already attached to the process");

				if (s_gpuHRESULT != S_OK)
				{
					const _com_error comError(s_gpuHRESULT);
					const std::string errorMessage = std::format("HRESULT error \"{}\" starting PIX GPU capture.\n"
						"Is PIX running in administrator mode, and attached to the process? Is only 1 command line "
						"argument supplied?",
						util::FromWideCString(comError.ErrorMessage()));

					bool showErrorPopup = true;
					util::ShowErrorPopup("Failed to start PIX GPU capture", errorMessage.c_str(), showErrorPopup);
					if (!showErrorPopup)
					{
						s_gpuHRESULT = S_OK;
					}
				}

				ImGui::InputText("Output path", s_pixGPUCapturePath, IM_ARRAYSIZE(s_pixGPUCapturePath));

				ImGui::SliderInt("No. of frames", &s_numPixGPUCaptureFrames, 1, 10);

				ImGui::EndDisabled();
			}

			ImGui::Separator();

			// CPU timing captures:
			if (pixCPUCaptureCmdLineEnabled)
			{
				ImGui::BeginDisabled(!pixCPUCaptureCmdLineEnabled);

				static char s_pixCPUCapturePath[256];
				static bool loadedPath = false;
				if (!loadedPath)
				{
					loadedPath = true;
					std::string const& pixFilePath = std::format("{}\\{}\\",
						core::Config::Get()->GetValueAsString(core::configkeys::k_documentsFolderPathKey),
						core::configkeys::k_pixCaptureFolderName);
					memcpy(s_pixCPUCapturePath, pixFilePath.c_str(), pixFilePath.length() + 1);
				}

				static bool s_captureGPUTimings = true;
				static bool s_captureCallstacks = true;
				static bool s_captureCpuSamples = true;
				static const uint32_t s_cpuSamplesPerSecond[3] = { 1000u, 4000u, 8000u };
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

					std::wstring const& filepath = util::ToWideString(
						std::format("{}\\{}TimingCapture_{}.wpix",
							s_pixCPUCapturePath,
							core::configkeys::k_captureTitle,
							util::GetTimeAndDateAsString()));

					PIXCaptureParameters pixCaptureParams = PIXCaptureParameters{
						.TimingCaptureParameters{
							.FileName = filepath.c_str(),

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
						"Is PIX running in administrator mode, and attached to the process? Is only 1 command line "
						"argument supplied?",
						util::FromWideCString(comError.ErrorMessage()));

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


	void RenderManager::ShowRenderDataImGuiWindow(bool* showRenderDataDebug) const
	{
		if (!*showRenderDataDebug)
		{
			return;
		}

		static const int windowWidth = core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
		static const int windowHeight = core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);
		constexpr float k_windowYOffset = 64.f;
		constexpr float k_windowWidthPercentage = 0.25f;

		ImGui::SetNextWindowSize(ImVec2(
			windowWidth * k_windowWidthPercentage,
			static_cast<float>(windowHeight) - k_windowYOffset),
			ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(0, k_windowYOffset), ImGuiCond_FirstUseEver, ImVec2(0, 0));

		constexpr char const* k_panelTitle = "Render Data Viewer";
		ImGui::Begin(k_panelTitle, showRenderDataDebug);

		m_renderData.ShowImGuiWindow();

		ImGui::End();
	}


	void RenderManager::ShowLightManagerImGuiWindow(bool* showLightMgrDebug) const
	{
		if (!*showLightMgrDebug)
		{
			return;
		}


		static const int windowWidth = core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
		static const int windowHeight = core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);
		constexpr float k_windowYOffset = 64.f;
		constexpr float k_windowWidthPercentage = 0.25f;

		ImGui::SetNextWindowSize(ImVec2(
			windowWidth * k_windowWidthPercentage,
			static_cast<float>(windowHeight) - k_windowYOffset),
			ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(0, k_windowYOffset), ImGuiCond_FirstUseEver, ImVec2(0, 0));

		constexpr char const* k_panelTitle = "Light manager debug";
		ImGui::Begin(k_panelTitle, showLightMgrDebug);

		m_lightManager.ShowImGuiWindow();

		ImGui::End();
	}
}


