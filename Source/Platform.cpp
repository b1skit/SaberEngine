// © 2022 Adam Badke. All rights reserved.
#include "Platform.h"

#include "Renderer/Context_DX12.h"
#include "Renderer/Context_OpenGL.h"
#include "Renderer/Context_Platform.h"

#include "Renderer/Buffer_DX12.h"
#include "Renderer/Buffer_OpenGL.h"
#include "Renderer/Buffer_Platform.h"
#include "Renderer/RLibrary_Platform.h"

#include "Renderer/RenderManager_DX12.h"
#include "Renderer/RenderManager_OpenGL.h"
#include "Renderer/RenderManager_Platform.h"

#include "Renderer/Sampler_DX12.h"
#include "Renderer/Sampler_OpenGL.h"
#include "Renderer/Sampler_Platform.h"

#include "Renderer/Shader_DX12.h"
#include "Renderer/Shader_OpenGL.h"
#include "Renderer/Shader_Platform.h"

#include "Renderer/SwapChain_DX12.h"
#include "Renderer/SwapChain_OpenGL.h"
#include "Renderer/SwapChain_Platform.h"

#include "Renderer/SysInfo_DX12.h"
#include "Renderer/SysInfo_OpenGL.h"
#include "Renderer/SysInfo_Platform.h"

#include "Renderer/Texture_DX12.h"
#include "Renderer/Texture_OpenGL.h"
#include "Renderer/Texture_Platform.h"

#include "Renderer/TextureTarget_DX12.h"
#include "Renderer/TextureTarget_OpenGL.h"
#include "Renderer/TextureTarget_Platform.h"

#include "Renderer/VertexStream_DX12.h"
#include "Renderer/VertexStream_OpenGL.h"
#include "Renderer/VertexStream_Platform.h"

#include "Renderer/Window_Platform.h"
#include "Renderer/Window_Win32.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/EventManager_Platform.h"
#include "Core/EventManager_Win32.h"
#include "Core/InputManager_Platform.h"
#include "Core/InputManager_Win32.h"


namespace platform
{
	constexpr char const* RenderingAPIToCStr(platform::RenderingAPI renderingAPI)
	{
		switch (renderingAPI)
		{
		case platform::RenderingAPI::OpenGL: return "OpenGL";
		case platform::RenderingAPI::DX12: return "DX12";
		default: SEAssertF("Invalid rendering API");
			return "INVALID";
		}
	}


	// Bind API-specific strategy implementations:
	bool RegisterPlatformFunctions()
	{
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		LOG("Configuring API-specific platform bindings...");

		bool result = true;

		// OS-Specific bindings (For now, we only support Windows):
		/*********************************************************/

		// Window:
		platform::Window::Create				= &win32::Window::Create;
		platform::Window::Destroy				= &win32::Window::Destroy;
		platform::Window::SetRelativeMouseMode	= &win32::Window::SetRelativeMouseMode;

		// Input manager:
		platform::InputManager::Startup				= &win32::InputManager::Startup;
		platform::InputManager::ConvertToSEKeycode	= &win32::InputManager::ConvertToSEKeycode;

		// Event manager:
		platform::EventManager::ProcessMessages	= &win32::EventManager::ProcessMessages;

		// Rendering API-specific bindings:
		/*********************************/
		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			// Context:
			platform::Context::Destroy	= &opengl::Context::Destroy;

			// Buffers:
			platform::Buffer::Create			= &opengl::Buffer::Create;
			platform::Buffer::Update			= &opengl::Buffer::Update;
			platform::Buffer::Destroy			= &opengl::Buffer::Destroy;
			platform::Buffer::MapCPUReadback	= &opengl::Buffer::MapCPUReadback;
			platform::Buffer::UnmapCPUReadback	= &opengl::Buffer::UnmapCPUReadback;

			// Render manager:
			platform::RenderManager::Initialize				= &opengl::RenderManager::Initialize;
			platform::RenderManager::Shutdown				= &opengl::RenderManager::Shutdown;
			platform::RenderManager::CreateAPIResources		= &opengl::RenderManager::CreateAPIResources;
			platform::RenderManager::GetNumFramesInFlight	= &opengl::RenderManager::GetNumFramesInFlight;

			// Shader:
			platform::Shader::Create	= &opengl::Shader::Create;
			platform::Shader::Destroy	= &opengl::Shader::Destroy;

			// SysInfo:
			platform::SysInfo::GetMaxRenderTargets		= &opengl::SysInfo::GetMaxRenderTargets;
			platform::SysInfo::GetMaxTextureBindPoints	= &opengl::SysInfo::GetMaxTextureBindPoints;

			// Swap chain:
			platform::SwapChain::Create			= &opengl::SwapChain::Create;
			platform::SwapChain::Destroy		= &opengl::SwapChain::Destroy;
			platform::SwapChain::SetVSyncMode	= &opengl::SwapChain::SetVSyncMode;

			// Texture:
			platform::Texture::Destroy	= &opengl::Texture::Destroy;

			// Texture Samplers:
			platform::Sampler::Create	= &opengl::Sampler::Create;
			platform::Sampler::Destroy	= &opengl::Sampler::Destroy;

			// Vertex stream:
			platform::VertexStream::CreatePlatformParams	= &opengl::VertexStream::CreatePlatformParams;
			platform::VertexStream::Destroy					= &opengl::VertexStream::Destroy;
		}
		break;
		case RenderingAPI::DX12:
		{
			// Context:
			platform::Context::Destroy	= &dx12::Context::Destroy;

			// Buffers:
			platform::Buffer::Create			= &dx12::Buffer::Create;
			platform::Buffer::Update			= &dx12::Buffer::Update;
			platform::Buffer::Destroy			= &dx12::Buffer::Destroy;
			platform::Buffer::MapCPUReadback	= &dx12::Buffer::MapCPUReadback;
			platform::Buffer::UnmapCPUReadback	= &dx12::Buffer::UnmapCPUReadback;
			
			// Render manager:
			platform::RenderManager::Initialize			= &dx12::RenderManager::Initialize;
			platform::RenderManager::Shutdown			= &dx12::RenderManager::Shutdown;
			platform::RenderManager::CreateAPIResources = &dx12::RenderManager::CreateAPIResources;
			platform::RenderManager::GetNumFramesInFlight		= &dx12::RenderManager::GetNumFramesInFlight;

			// Shader:
			platform::Shader::Create	= &dx12::Shader::Create;
			platform::Shader::Destroy	= &dx12::Shader::Destroy;

			// SysInfo:
			platform::SysInfo::GetMaxRenderTargets		= &dx12::SysInfo::GetMaxRenderTargets;
			platform::SysInfo::GetMaxTextureBindPoints	= &dx12::SysInfo::GetMaxTextureBindPoints;

			// Swap chain:
			platform::SwapChain::Create			= &dx12::SwapChain::Create;
			platform::SwapChain::Destroy		= &dx12::SwapChain::Destroy;
			platform::SwapChain::SetVSyncMode	= &dx12::SwapChain::SetVSyncMode;

			// Texture:
			platform::Texture::Destroy	= &dx12::Texture::Destroy;

			// Texture Samplers:
			platform::Sampler::Create	= &dx12::Sampler::Create;
			platform::Sampler::Destroy	= &dx12::Sampler::Destroy;

			// Vertex stream:
			platform::VertexStream::CreatePlatformParams	= &dx12::VertexStream::CreatePlatformParams;
			platform::VertexStream::Destroy					= &dx12::VertexStream::Destroy;
		}
		break;
		default:
		{
			SEAssertF("Unsupported rendering API");
			result = false;
		}
		}

		// Handle render layer library bindings:
		if (result)
		{
			result &= platform::RLibrary::RegisterPlatformLibraries();
		}

		LOG("Done!");

		return result;
	}
}
