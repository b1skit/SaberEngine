// � 2022 Adam Badke. All rights reserved.
#include "Context.h"
#include "Context_DX12.h"
#include "Context_OpenGL.h"
#include "EnumTypes.h"
#include "Sampler.h"
#include "Texture.h"
#include "VertexStream.h"

#include "Core/AccessKey.h"
#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/Inventory.h"
#include "Core/Invptr.h"
#include "Core/PerfLogger.h"
#include "Core/ProfilingMarkers.h"

#include "Core/Interfaces/ILoadContext.h"
#include "Core/Interfaces/IPlatformObject.h"

#include "Core/Util/TextUtils.h"


namespace re
{
	std::unique_ptr<re::Context> Context::CreateContext_Platform(
		platform::RenderingAPI api,
		uint64_t currentFrameNum,
		uint8_t numFramesInFlight,
		host::Window* window)
	{
		SEAssert(window, "Received a null window");

		std::unique_ptr<re::Context> newContext = nullptr;
		switch (api)
		{
		case platform::RenderingAPI::OpenGL:
		{
			newContext.reset(new opengl::Context(api, numFramesInFlight, window));
		}
		break;
		case platform::RenderingAPI::DX12:
		{
			newContext.reset(new dx12::Context(api, numFramesInFlight, window));
		}
		break;
		default: SEAssertF("Invalid rendering API argument received");
		}

		newContext->Create(currentFrameNum);

		return newContext;
	}


	Context::Context(
		platform::RenderingAPI api, uint8_t numFramesInFlight, host::Window* window)
		: m_newShaders(util::NBufferedVector<core::InvPtr<re::Shader>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newTextures(util::NBufferedVector<core::InvPtr<re::Texture>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newSamplers(util::NBufferedVector<core::InvPtr<re::Sampler>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newVertexStreams(util::NBufferedVector<core::InvPtr<re::VertexStream>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newAccelerationStructures(util::NBufferedVector<std::shared_ptr<re::AccelerationStructure>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newShaderBindingTables(util::NBufferedVector<std::shared_ptr<re::ShaderBindingTable>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_newTargetSets(util::NBufferedVector<std::shared_ptr<re::TextureTargetSet>>::BufferSize::Two, k_newObjectReserveAmount)
		, m_window(window)
		, m_gpuTimer(core::PerfLogger::Get(), numFramesInFlight)
		, m_numFramesInFlight(numFramesInFlight)
		, m_renderDocApi(nullptr)
		, m_currentFrameNum(std::numeric_limits<uint64_t>::max())
	{
		SEAssert(m_window, "Received a null window");

		// RenderDoc cannot be enabled when DRED is enabled
		const bool dredEnabled = core::Config::KeyExists(core::configkeys::k_enableDredCmdLineArg);

		const bool enableRenderDocProgrammaticCaptures =
			core::Config::KeyExists(core::configkeys::k_renderDocProgrammaticCapturesCmdLineArg);

		if (enableRenderDocProgrammaticCaptures && dredEnabled)
		{
			LOG_ERROR("RenderDoc and DRED cannot be enabled at the same time. RenderDoc will not be enabled");
		}
		else if(enableRenderDocProgrammaticCaptures && !dredEnabled)
		{
			LOG("Loading renderdoc.dll...");

			HMODULE renderDocModule = LoadLibraryA("renderdoc.dll");
			if (renderDocModule)
			{
				LOG("Successfully loaded renderdoc.dll");

				pRENDERDOC_GetAPI RENDERDOC_GetAPI = 
					(pRENDERDOC_GetAPI)GetProcAddress(renderDocModule, "RENDERDOC_GetAPI");
				int result = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&m_renderDocApi);
				SEAssert(result == 1, "Failed to get the RenderDoc API");

				// Set the capture options before the graphics API is initialized:
				int captureOptionResult = 
					m_renderDocApi->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_AllowVSync, 1);

				captureOptionResult =
					m_renderDocApi->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_AllowFullscreen, 1);

				// Don't capture callstacks (for now)
				captureOptionResult =
					m_renderDocApi->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_CaptureCallstacks, 0);

				captureOptionResult =
					m_renderDocApi->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_CaptureCallstacksOnlyActions, 0);

				if (core::Config::GetValue<int>(core::configkeys::k_debugLevelCmdLineArg) >= 1)
				{
					captureOptionResult =
						m_renderDocApi->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_APIValidation, 1);

					captureOptionResult =
						m_renderDocApi->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_VerifyBufferAccess, 1);
				}

				// Only include resources necessary for the final capture (for now)
				captureOptionResult =
					m_renderDocApi->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_RefAllResources, 0);

				// Set the default output folder/file path. RenderDoc appends "_frameXYZ.rdc" to the end
				std::string const& renderDocCapturePath = std::format("{}\\{}\\{}_{}_{}",
					core::Config::GetValueAsString(core::configkeys::k_documentsFolderPathKey),
					core::configkeys::k_renderDocCaptureFolderName,
					core::configkeys::k_captureTitle,
					platform::RenderingAPIToCStr(api),
					util::GetTimeAndDateAsString());
				m_renderDocApi->SetCaptureFilePathTemplate(renderDocCapturePath.c_str());
			}
			else
			{
				const HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
				const _com_error comError(hr);
				LOG_ERROR(std::format("HRESULT error loading RenderDoc module: \"{}\"",
					util::FromWideCString(comError.ErrorMessage())).c_str());
			}
		}

		core::IPlatObj::s_context = this;
		core::ILoadContextBase::s_context = this;
	}


	void Context::Create(uint64_t currentFrame)
	{
		m_currentFrameNum = currentFrame;

		Create_Platform();

		CreateSamplerLibrary();
		CreateDefaultTextures();

		m_swapChain.Create(re::Texture::Format::RGBA8_UNORM, m_numFramesInFlight, this);

		// Trigger creation of render libraries:
		for (uint8_t i = 0; i < platform::RLibrary::Type::Type_Count; ++i)
		{
			GetOrCreateRenderLibrary(static_cast<platform::RLibrary::Type>(i));
		}
	}


	void Context::BeginFrame(uint64_t currentFrame)
	{
		SEAssert(m_currentFrameNum == 0 || // First frame: We already set m_currentFrameNum in Create()
			currentFrame == m_currentFrameNum + 1,
			"Frame numbers are out of sync");

		m_currentFrameNum = currentFrame;

		m_gpuTimer.BeginFrame(m_currentFrameNum);

		m_bufferAllocator->BeginFrame(m_currentFrameNum);

		if (re::BindlessResourceManager* brm = GetBindlessResourceManager())
		{
			brm->BeginFrame(m_currentFrameNum);
		}

		BeginFrame_Platform();
	}


	void Context::Update()
	{
		SEBeginCPUEvent("re::Context::Update");

		// Clear our cache of new objects, now that our anything that needs them has had a chance to access them.
		ClearNewObjectCache();

		// Create any new resources that have been created by GS's during the ExecuteUpdatePipeline call:
		CreateAPIResources();

		// Ensure any new buffer objects have their platform-level resources created:
		m_bufferAllocator->CreateBufferPlatformObjects();

		// Platform-level updates:
		SEBeginCPUEvent("re::Context::UpdateInternal");
		Update_Platform();
		SEEndCPUEvent();

		// Commit buffer data immediately before rendering
		m_bufferAllocator->BufferData();

		SEEndCPUEvent();
	}


	void Context::EndFrame()
	{
		// Clear the new resource read data: This prevents any single frame resources held by the NBufferedVectors
		// living into the next frame
		m_newShaders.ClearReadData();
		m_newTextures.ClearReadData();
		m_newSamplers.ClearReadData();
		m_newVertexStreams.ClearReadData();
		m_newAccelerationStructures.ClearReadData();
		m_newShaderBindingTables.ClearReadData();
		m_newTargetSets.ClearReadData();

		ProcessDeferredDeletions(m_currentFrameNum);

		EndFrame_Platform();

		// Free Resources that have gone out of scope
		core::Inventory::OnEndOfFrame(ACCESS_KEY(core::Inventory::AccessKey));
	}


	void Context::Destroy()
	{
		m_defaultTextures.clear();

		// Destroy the swap chain before forcing deferred deletions. This is safe, as we've already flushed any
		// remaining outstanding work
		m_swapChain.Destroy();

		// We destroy this on behalf of the EngineApp, as the inventory typically contains GPU resources that need to
		// be destroyed from the render thread (i.e. for OpenGL)
		core::Inventory::Destroy(ACCESS_KEY(core::Inventory::AccessKey));

		// Destroy the BufferAllocator before we process deferred deletions, as Buffers free their PlatObj there
		m_bufferAllocator->Destroy();

		m_createdTextures.clear();

		// Clear the new object queues:
		DestroyNewResourceDoubleBuffers();

		// Destroy any render libraries
		for (size_t i = 0; i < m_renderLibraries.size(); i++)
		{
			if (m_renderLibraries[i] != nullptr)
			{
				m_renderLibraries[i]->Destroy();
				m_renderLibraries[i] = nullptr;
			}
		}
		m_gpuTimer.Destroy();
		
		ProcessDeferredDeletions(k_forceDeferredDeletionsFlag); // Force-delete everything

		Destroy_Platform();

		core::IPlatObj::s_context = nullptr;
		core::ILoadContextBase::s_context = nullptr;
	}


	platform::RLibrary* Context::GetOrCreateRenderLibrary(platform::RLibrary::Type type)
	{
		if (m_renderLibraries[type] == nullptr)
		{
			m_renderLibraries[type] = platform::RLibrary::Create(type);
		}
		return m_renderLibraries[type].get();
	}


	void Context::CreateAPIResources()
	{
		SEBeginCPUEvent("platform::Context::CreateAPIResources");

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
		CreateAPIResources_Platform();

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


	void Context::ClearNewObjectCache()
	{
		SEBeginCPUEvent("Context::ClearNewObjectCache");

		// Clear the initial data of our new textures now that they have been buffered
		for (auto const& newTexture : m_createdTextures)
		{
			newTexture->ClearTexelData();
		}

		// Clear any objects created during the frame. We do this each frame after the RenderSystem updates to
		// ensure anything that needs to know about new objects being created (e.g. MIP generation GS) can see them
		m_createdTextures.clear();

		SEEndCPUEvent();
	}


	void Context::SwapNewResourceDoubleBuffers()
	{
		SEBeginCPUEvent("Context::SwapNewResourceDoubleBuffers");

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


	void Context::DestroyNewResourceDoubleBuffers()
	{
		m_newShaders.Destroy();
		m_newTextures.Destroy();
		m_newSamplers.Destroy();
		m_newVertexStreams.Destroy();
		m_newAccelerationStructures.Destroy();
		m_newShaderBindingTables.Destroy();
		m_newTargetSets.Destroy();
	}


	void Context::CreateSamplerLibrary()
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


	void Context::CreateDefaultTextures()
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
			.m_usage = re::Texture::Usage::ColorSrc | re::Texture::Usage::ColorTarget,
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



	template<>
	void Context::RegisterForCreate(core::InvPtr<re::Shader> const& newObject)
	{
		m_newShaders.EmplaceBack(newObject);
	}


	template<>
	void Context::RegisterForCreate(core::InvPtr<re::Texture> const& newObject)
	{
		m_newTextures.EmplaceBack(newObject);
	}


	template<>
	void Context::RegisterForCreate(core::InvPtr<re::Sampler> const& newObject)
	{
		m_newSamplers.EmplaceBack(newObject);
	}


	template<>
	void Context::RegisterForCreate(core::InvPtr<re::VertexStream> const& newObject)
	{
		m_newVertexStreams.EmplaceBack(newObject);
	}


	template<>
	void Context::RegisterForCreate(std::shared_ptr<re::AccelerationStructure> const& newObject)
	{
		m_newAccelerationStructures.EmplaceBack(newObject);
	}


	template<>
	void Context::RegisterForCreate(std::shared_ptr<re::ShaderBindingTable> const& newObject)
	{
		m_newShaderBindingTables.EmplaceBack(newObject);
	}


	template<>
	void Context::RegisterForCreate(std::shared_ptr<re::TextureTargetSet> const& newObject)
	{
		m_newTargetSets.EmplaceBack(newObject);
	}


	template<>
	std::vector<core::InvPtr<re::Texture>> const& Context::GetNewResources() const
	{
		return m_createdTextures;
	}


	void Context::RegisterForDeferredDelete(std::unique_ptr<core::IPlatObj>&& platObj)
	{
		{
			std::lock_guard<std::mutex> lock(m_deletedPlatObjectsMutex);

			m_deletedPlatObjects.emplace(PlatformDeferredDelete{
				.m_platObj = std::move(platObj),
				.m_frameNum = m_currentFrameNum });
		}
	}


	void Context::ProcessDeferredDeletions(uint64_t frameNum)
	{
		const uint8_t numFramesInFlight = m_numFramesInFlight;
		{
			std::lock_guard<std::mutex> lock(m_deletedPlatObjectsMutex);

			while (!m_deletedPlatObjects.empty() &&
				m_deletedPlatObjects.front().m_frameNum + numFramesInFlight < frameNum)
			{
				m_deletedPlatObjects.front().m_platObj->Destroy();
				m_deletedPlatObjects.pop();
			}
		}
	}
}