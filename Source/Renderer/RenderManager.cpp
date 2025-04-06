// © 2022 Adam Badke. All rights reserved.
#include "AccelerationStructure.h"
#include "GraphicsSystemManager.h"
#include "RenderManager.h"
#include "RenderManager_DX12.h"
#include "RenderManager_Platform.h"
#include "RenderManager_OpenGL.h"
#include "Sampler.h"
#include "ShaderBindingTable.h"
#include "TextureTarget.h"
#include "VertexStream.h"

#include "Core/Config.h"
#include "Core/PerfLogger.h"
#include "Core/ProfilingMarkers.h"

#include "Core/Definitions/EventKeys.h"

#include "Core/Host/PerformanceTimer.h"

#include "Core/Util/FileIOUtils.h"


namespace re
{
	constexpr char const* k_renderThreadLogName = "Render thread";


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

		// Validate the shader directory build configuration file matches the current compiled build configuration:
		const util::BuildConfiguration buildConfig = util::GetBuildConfigurationMarker(
			config->GetValueAsString(core::configkeys::k_shaderDirectoryKey));
		
#if defined(SE_DEBUG)
		SEFatalAssert(buildConfig == util::BuildConfiguration::Debug, "Shader directory build configuration marker mismatch");
#elif defined(SE_DEBUGRELEASE)
		SEFatalAssert(buildConfig == util::BuildConfiguration::DebugRelease, "Shader directory build configuration marker mismatch");
#elif defined(SE_PROFILE)
		SEFatalAssert(buildConfig == util::BuildConfiguration::Profile, "Shader directory build configuration marker mismatch");
#elif defined(SE_RELEASE)
		SEFatalAssert(buildConfig == util::BuildConfiguration::Release, "Shader directory build configuration marker mismatch");
#endif

		return newRenderManager;
	}


	RenderManager::RenderManager(platform::RenderingAPI renderingAPI)
		: m_renderingAPI(renderingAPI)
		, m_renderFrameNum(0)
		, m_renderCommandManager(k_renderCommandBufferSize)
		, m_inventory(nullptr)
		, m_newShaders(util::NBufferedVector<core::InvPtr<re::Shader>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newTextures(util::NBufferedVector<core::InvPtr<re::Texture>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newSamplers(util::NBufferedVector<core::InvPtr<re::Sampler>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newVertexStreams(util::NBufferedVector<core::InvPtr<gr::VertexStream>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newAccelerationStructures(util::NBufferedVector<std::shared_ptr<re::AccelerationStructure>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newShaderBindingTables(util::NBufferedVector<std::shared_ptr<re::ShaderBindingTable>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newTargetSets(util::NBufferedVector<std::shared_ptr<re::TextureTargetSet>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_quitEventRecieved(false)
	{
	}


	void RenderManager::Lifetime(std::barrier<>* syncBarrier)
	{
		// Synchronized startup: Blocks main thread until complete
		m_startupLatch[static_cast<size_t>(SyncType::ReleaseWorker)].arrive_and_wait();
		Startup();
		m_startupLatch[static_cast<size_t>(SyncType::ReleaseCommander)].arrive_and_wait();

		// Synchronized initialization: Blocks main thread until complete
		m_initializeLatch[static_cast<size_t>(SyncType::ReleaseWorker)].arrive_and_wait();
		Initialize();
		m_initializeLatch[static_cast<size_t>(SyncType::ReleaseCommander)].arrive_and_wait();

		core::PerfLogger* perfLogger = core::PerfLogger::Get();

		IEngineThread::ThreadUpdateParams updateParams{};

		m_isRunning = true;
		while (m_isRunning)
		{
			// Blocks until updateParams is updated, or the IEngineThread has been signaled to stop
			if (!GetUpdateParams(updateParams))
			{
				break;
			}

			SEBeginCPUEvent("RenderManager frame");
			perfLogger->NotifyBegin(k_renderThreadLogName);

			m_renderFrameNum = updateParams.m_frameNum;

			BeginFrame(m_renderFrameNum);

			const std::barrier<>::arrival_token& copyArrive = syncBarrier->arrive();

			Update(m_renderFrameNum, updateParams.m_elapsed);

			EndFrame(); // Clear batches, process pipeline and buffer allocator EndOfFrames

			perfLogger->NotifyEnd(k_renderThreadLogName);
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
		
		core::EventManager::Get()->Subscribe(eventkey::ToggleVSync, this);
		core::EventManager::Get()->Subscribe(eventkey::EngineQuit, this);
		
		CreateSamplerLibrary();
		CreateDefaultTextures();

		// Trigger creation of render libraries:
		for (uint8_t i = 0; i < platform::RLibrary::Type::Type_Count; ++i)
		{
			re::Context::Get()->GetOrCreateRenderLibrary(static_cast<platform::RLibrary::Type>(i));
		}

		SEEndCPUEvent();
	}
	
	
	void RenderManager::Initialize()
	{
		SEBeginCPUEvent("re::RenderManager::Initialize");

		LOG("RenderManager Initializing...");
		host::PerformanceTimer timer;
		timer.Start();

		SEAssert(m_inventory, "Inventory is null. This dependency must be injected immediately after creation");

		m_effectDB.LoadEffectManifest();

		SEBeginCPUEvent("platform::RenderManager::Initialize");
		platform::RenderManager::Initialize(*this);
		SEEndCPUEvent();

		LOG("\nRenderManager::Initialize complete in %f seconds...\n", timer.StopSec());

		SEEndCPUEvent();
	}


	gr::RenderSystem const* RenderManager::CreateAddRenderSystem(std::string const& pipelineFileName)
	{
		m_renderSystems.emplace_back(gr::RenderSystem::Create(pipelineFileName));

		// Initialize the render system (which will in turn initialize each of its graphics systems & stage pipelines)
		m_renderSystems.back()->ExecuteInitializationPipeline();

		return m_renderSystems.back().get();
	}


	void RenderManager::BeginFrame(uint64_t frameNum)
	{
		SEBeginCPUEvent("re::RenderManager::BeginFrame");
		
		m_renderCommandManager.SwapBuffers();

		platform::RenderManager::BeginFrame(*this, frameNum);

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
		
		// Get the RenderDataManager ready for the new frame
		m_renderData.BeginFrame(m_renderFrameNum);

		m_renderCommandManager.Execute(); // Process render commands. Must happen 1st to ensure RenderData is up to date

		re::Context* context = re::Context::Get();

		context->GetGPUTimer().BeginFrame(m_renderFrameNum); // Platform layers internally call GPUTimer::EndFrame()

		// We must create any API resources that were passed via render commands, as they may be required during GS
		// updates (e.g. MeshPrimitive VertexStream Buffer members need to be created so we can set them on BufferInputs)
		// TODO: Remove this once we have Buffer handles
		CreateAPIResources();

		// Execute each RenderSystem's platform-specific graphics system update pipelines:
		SEBeginCPUEvent("Execute update pipeline");
		for (std::unique_ptr<gr::RenderSystem>& renderSystem : m_renderSystems)
		{
			renderSystem->ExecuteUpdatePipeline();
			renderSystem->PostUpdatePreRender();
		}
		SEEndCPUEvent();

		// Clear our cache of new objects, now that our anything that needs them has had a chance to access them.
		ClearNewObjectCache();

		// Create any new resources that have been created by GS's during the ExecuteUpdatePipeline call:
		CreateAPIResources();

		// Update context objects (Buffers, BindlessResourceManager, etc)
		context->Update(frameNum);

		// API-specific rendering loop virtual implementations:
		SEBeginCPUEvent("platform::RenderManager::Render");
		Render();
		SEEndCPUEvent();

		// Present the finished frame:
		SEBeginCPUEvent("re::Context::Present");
		context->Present();
		SEEndCPUEvent();

		SEEndCPUEvent();
	}


	void RenderManager::EndFrame()
	{
		SEBeginCPUEvent("re::RenderManager::EndFrame");

		SEBeginCPUEvent("Process render systems");
		{
			for (std::unique_ptr<gr::RenderSystem>& renderSystem : m_renderSystems)
			{
				renderSystem->EndOfFrame();
			}
		}
		SEEndCPUEvent();
		
		ProcessDeferredDeletions(GetCurrentRenderFrameNum());

		platform::RenderManager::EndFrame(*this);

		SEEndCPUEvent();
	}


	void RenderManager::RegisterForDeferredDelete(std::unique_ptr<core::IPlatformParams>&& platParams)
	{
		{
			std::lock_guard<std::mutex> lock(m_deletedPlatObjectsMutex);

			m_deletedPlatObjects.emplace(PlatformDeferredDelete{
				.m_platformParams = std::move(platParams),
				.m_frameNum = GetCurrentRenderFrameNum() });
		}
	}


	void RenderManager::ProcessDeferredDeletions(uint64_t frameNum)
	{
		const uint8_t numFramesInFlight = GetNumFramesInFlight();
		{
			std::lock_guard<std::mutex> lock(m_deletedPlatObjectsMutex);

			while (!m_deletedPlatObjects.empty() &&
				m_deletedPlatObjects.front().m_frameNum + numFramesInFlight < frameNum)
			{
				m_deletedPlatObjects.front().m_platformParams->Destroy();
				m_deletedPlatObjects.pop();
			}
		}
	}


	void RenderManager::Shutdown()
	{
		SEBeginCPUEvent("re::RenderManager::Shutdown");

		LOG("Render manager shutting down...");

		re::Context* context = re::Context::Get();

		// Flush any remaining render work:
		platform::RenderManager::Shutdown(*this);

		// Process any remaining render commands (i.e. delete platform objects)
		m_renderCommandManager.SwapBuffers();
		m_renderCommandManager.Execute();

		m_effectDB.Destroy();
	
		// Destroy render systems:
		for (std::unique_ptr<gr::RenderSystem>& renderSystem : m_renderSystems)
		{
			renderSystem->Destroy();
		}
		m_renderSystems.clear();

		m_renderData.Destroy();

		m_createdTextures.clear();

		// Clear the new object queues:
		DestroyNewResourceDoubleBuffers();

		m_defaultTextures.clear();

		// Destroy the swap chain before forcing deferred deletions. This is safe, as we've already flushed any
		// remaining outstanding work
		context->GetSwapChain().Destroy();

		// We destroy this on behalf of the EngineApp, as the inventory typically contains GPU resources that need to
		// be destroyed from the render thread (i.e. for OpenGL)
		m_inventory->Destroy();

		// Destroy the BufferAllocator before we process deferred deletions, as Buffers free their PlatformParams there
		context->GetBufferAllocator()->Destroy();

		ProcessDeferredDeletions(k_forceDeferredDeletionsFlag); // Force-delete everything

		// Need to do this here so the EngineApp's Window can be destroyed
		context->Destroy();

		SEEndCPUEvent();
	}


	void RenderManager::HandleEvents()
	{
		SEBeginCPUEvent("re::RenderManager::HandleEvents");

		while (HasEvents())
		{
			core::EventManager::EventInfo const& eventInfo = GetEvent();

			switch (eventInfo.m_eventKey)
			{
			case eventkey::ToggleVSync:
			{
				re::Context::Get()->GetSwapChain().ToggleVSync();
			}
			break;
			case eventkey::EngineQuit:
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
		m_newTextures.AquireReadLock();
		m_newSamplers.AquireReadLock();
		m_newVertexStreams.AquireReadLock();
		m_newAccelerationStructures.AquireReadLock();
		m_newShaderBindingTables.AquireReadLock();
		m_newTargetSets.AquireReadLock();
		

		// Record newly created objects. This provides an convenient way to post-process new objects
		{
			m_createdTextures.reserve(m_createdTextures.size() + m_newTextures.GetReadData().size());
			m_createdTextures.insert(
				m_createdTextures.end(),
				m_newTextures.GetReadData().begin(),
				m_newTextures.GetReadData().end());
		}

		// Create the resources:
		platform::RenderManager::CreateAPIResources(*this);

		// Release read locks:
		m_newShaders.ReleaseReadLock();
		m_newTextures.ReleaseReadLock();
		m_newSamplers.ReleaseReadLock();
		m_newVertexStreams.ReleaseReadLock();
		m_newAccelerationStructures.ReleaseReadLock();
		m_newShaderBindingTables.ReleaseReadLock();
		m_newTargetSets.ReleaseReadLock();

		SEEndCPUEvent();
	}


	void RenderManager::ClearNewObjectCache()
	{
		// Clear the initial data of our new textures now that they have been buffered
		for (auto const& newTexture : m_createdTextures)
		{
			newTexture->ClearTexelData();
		}

		// Clear any objects created during the frame. We do this each frame after the RenderSystem updates to
		// ensure anything that needs to know about new objects being created (e.g. MIP generation GS) can see them
		m_createdTextures.clear();
	}


	void RenderManager::SwapNewResourceDoubleBuffers()
	{
		SEBeginCPUEvent("RenderManager::SwapNewResourceDoubleBuffers");

		// Swap our new resource double buffers:
		m_newShaders.SwapAndClear();
		m_newTextures.SwapAndClear();
		m_newSamplers.SwapAndClear();
		m_newVertexStreams.SwapAndClear();
		m_newAccelerationStructures.SwapAndClear();
		m_newShaderBindingTables.SwapAndClear();
		m_newTargetSets.SwapAndClear();

		SEEndCPUEvent();
	}


	void RenderManager::DestroyNewResourceDoubleBuffers()
	{
		m_newShaders.Destroy();
		m_newTextures.Destroy();
		m_newSamplers.Destroy();
		m_newVertexStreams.Destroy();
		m_newAccelerationStructures.Destroy();
		m_newShaderBindingTables.Destroy();
		m_newTargetSets.Destroy();
	}


	template<>
	void RenderManager::RegisterForCreate(core::InvPtr<re::Shader> const& newObject)
	{
		m_newShaders.EmplaceBack(newObject);
	}


	template<>
	void RenderManager::RegisterForCreate(core::InvPtr<re::Texture> const& newObject)
	{
		m_newTextures.EmplaceBack(newObject);
	}


	template<>
	void RenderManager::RegisterForCreate(core::InvPtr<re::Sampler> const& newObject)
	{
		m_newSamplers.EmplaceBack(newObject);
	}


	template<>
	void RenderManager::RegisterForCreate(core::InvPtr<gr::VertexStream> const& newObject)
	{
		m_newVertexStreams.EmplaceBack(newObject);
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::AccelerationStructure> const& newObject)
	{
		m_newAccelerationStructures.EmplaceBack(newObject);
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::ShaderBindingTable> const& newObject)
	{
		m_newShaderBindingTables.EmplaceBack(newObject);
	}


	template<>
	void RenderManager::RegisterForCreate(std::shared_ptr<re::TextureTargetSet> const& newObject)
	{
		m_newTargetSets.EmplaceBack(newObject);
	}


	template<>
	std::vector<core::InvPtr<re::Texture>> const& RenderManager::GetNewResources() const
	{
		return m_createdTextures;
	}


	void RenderManager::CreateSamplerLibrary()
	{
		// Internally, our Samplers will permanently self-register with the inventory, we're just triggering that here

		constexpr re::Sampler::SamplerDesc k_wrapMinMagLinearMipPoint = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::MIN_MAG_LINEAR_MIP_POINT,
			.m_edgeModeU = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeV = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeW = re::Sampler::EdgeMode::Wrap,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
			.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("WrapMinMagLinearMipPoint", k_wrapMinMagLinearMipPoint);

		constexpr re::Sampler::SamplerDesc k_clampMinMagLinearMipPoint = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::MIN_MAG_LINEAR_MIP_POINT,
			.m_edgeModeU = re::Sampler::EdgeMode::Clamp,
			.m_edgeModeV = re::Sampler::EdgeMode::Clamp,
			.m_edgeModeW = re::Sampler::EdgeMode::Clamp,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
			.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("ClampMinMagLinearMipPoint", k_clampMinMagLinearMipPoint);

		constexpr re::Sampler::SamplerDesc k_clampMinMagMipPoint = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::MIN_MAG_MIP_POINT,
			.m_edgeModeU = re::Sampler::EdgeMode::Clamp,
			.m_edgeModeV = re::Sampler::EdgeMode::Clamp,
			.m_edgeModeW = re::Sampler::EdgeMode::Clamp,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
			.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("ClampMinMagMipPoint", k_clampMinMagMipPoint);

		constexpr re::Sampler::SamplerDesc k_borderMinMagMipPoint = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::MIN_MAG_MIP_POINT,
			.m_edgeModeU = re::Sampler::EdgeMode::Border,
			.m_edgeModeV = re::Sampler::EdgeMode::Border,
			.m_edgeModeW = re::Sampler::EdgeMode::Border,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
			.m_borderColor = re::Sampler::BorderColor::OpaqueWhite,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("WhiteBorderMinMagMipPoint", k_borderMinMagMipPoint);

		constexpr re::Sampler::SamplerDesc k_clampMinMagMipLinear = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::MIN_MAG_MIP_LINEAR,
			.m_edgeModeU = re::Sampler::EdgeMode::Clamp,
			.m_edgeModeV = re::Sampler::EdgeMode::Clamp,
			.m_edgeModeW = re::Sampler::EdgeMode::Clamp,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
			.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("ClampMinMagMipLinear", k_clampMinMagMipLinear);

		constexpr re::Sampler::SamplerDesc k_wrapMinMagMipLinear = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::MIN_MAG_MIP_LINEAR,
			.m_edgeModeU = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeV = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeW = re::Sampler::EdgeMode::Wrap,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
			.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("WrapMinMagMipLinear", k_wrapMinMagMipLinear);

		constexpr re::Sampler::SamplerDesc k_wrapAnisotropic = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::ANISOTROPIC,
			.m_edgeModeU = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeV = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeW = re::Sampler::EdgeMode::Wrap,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
			.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("WrapAnisotropic", k_wrapAnisotropic);

		// PCF Samplers
		constexpr float k_maxLinearDepth = std::numeric_limits<float>::max();

		constexpr re::Sampler::SamplerDesc k_borderCmpMinMagLinearMipPoint = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
			.m_edgeModeU = re::Sampler::EdgeMode::Border,
			.m_edgeModeV = re::Sampler::EdgeMode::Border,
			.m_edgeModeW = re::Sampler::EdgeMode::Border,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::Less,
			.m_borderColor = re::Sampler::BorderColor::OpaqueWhite,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("BorderCmpMinMagLinearMipPoint", k_borderCmpMinMagLinearMipPoint);

		constexpr re::Sampler::SamplerDesc k_wrapCmpMinMagLinearMipPoint = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
			.m_edgeModeU = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeV = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeW = re::Sampler::EdgeMode::Wrap,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::Less,
			.m_borderColor = re::Sampler::BorderColor::OpaqueWhite,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("WrapCmpMinMagLinearMipPoint", k_wrapCmpMinMagLinearMipPoint);
	}


	void RenderManager::CreateDefaultTextures()
	{
		// Default 2D texture fallbacks:
		const re::Texture::TextureParams defaultTexParams = re::Texture::TextureParams
		{
			.m_usage = re::Texture::Usage::ColorSrc,
			.m_dimension = re::Texture::Dimension::Texture2D,
			.m_format = re::Texture::Format::RGBA8_UNORM,
			.m_colorSpace = re::Texture::ColorSpace::Linear,
			.m_mipMode = re::Texture::MipMode::None,
			.m_createAsPermanent = true,
		};

		m_defaultTextures[en::DefaultResourceNames::k_opaqueWhiteDefaultTexName] = re::Texture::Create(
			en::DefaultResourceNames::k_opaqueWhiteDefaultTexName,
			defaultTexParams,
			glm::vec4(1.f, 1.f, 1.f, 1.f));

		m_defaultTextures[en::DefaultResourceNames::k_transparentWhiteDefaultTexName] = re::Texture::Create(
			en::DefaultResourceNames::k_transparentWhiteDefaultTexName,
			defaultTexParams,
			glm::vec4(1.f, 1.f, 1.f, 0.f));

		m_defaultTextures[en::DefaultResourceNames::k_opaqueBlackDefaultTexName] = re::Texture::Create(
			en::DefaultResourceNames::k_opaqueBlackDefaultTexName,
			defaultTexParams,
			glm::vec4(0.f, 0.f, 0.f, 1.f));

		m_defaultTextures[en::DefaultResourceNames::k_transparentBlackDefaultTexName] = re::Texture::Create(
			en::DefaultResourceNames::k_transparentBlackDefaultTexName,
			defaultTexParams,
			glm::vec4(0.f, 0.f, 0.f, 0.f));


		// Default cube map texture fallbacks:
		const re::Texture::TextureParams defaultCubeMapTexParams = re::Texture::TextureParams
		{
			.m_usage = static_cast<re::Texture::Usage>(re::Texture::Usage::ColorSrc | re::Texture::Usage::ColorTarget),
			.m_dimension = re::Texture::Dimension::TextureCube,
			.m_format = re::Texture::Format::RGBA8_UNORM,
			.m_colorSpace = re::Texture::ColorSpace::Linear,
			.m_createAsPermanent = true,
		};

		m_defaultTextures[en::DefaultResourceNames::k_cubeMapOpaqueWhiteDefaultTexName] = re::Texture::Create(
			en::DefaultResourceNames::k_cubeMapOpaqueWhiteDefaultTexName,
			defaultCubeMapTexParams,
			glm::vec4(1.f, 1.f, 1.f, 1.f));

		m_defaultTextures[en::DefaultResourceNames::k_cubeMapTransparentWhiteDefaultTexName] = re::Texture::Create(
			en::DefaultResourceNames::k_cubeMapTransparentWhiteDefaultTexName,
			defaultCubeMapTexParams,
			glm::vec4(1.f, 1.f, 1.f, 0.f));

		m_defaultTextures[en::DefaultResourceNames::k_cubeMapOpaqueBlackDefaultTexName] = re::Texture::Create(
			en::DefaultResourceNames::k_cubeMapOpaqueBlackDefaultTexName,
			defaultCubeMapTexParams,
			glm::vec4(0.f, 0.f, 0.f, 1.f));

		m_defaultTextures[en::DefaultResourceNames::k_cubeMapTransparentBlackDefaultTexName] = re::Texture::Create(
			en::DefaultResourceNames::k_cubeMapTransparentBlackDefaultTexName,
			defaultCubeMapTexParams,
			glm::vec4(0.f, 0.f, 0.f, 0.f));
	}


	void RenderManager::ShowRenderSystemsImGuiWindow(bool* show)
	{
		if (!(*show))
		{
			return;
		}

		ImGui::Begin(std::format("Render Systems ({})", m_renderSystems.size()).c_str(), show);

		// Render systems:
		for (std::unique_ptr<gr::RenderSystem>& renderSystem : m_renderSystems)
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

		ImGui::End();
	}
}


