// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Config.h"
#include "Platform.h"

#include "Context_DX12.h"
#include "Context_OpenGL.h"
#include "Context_Platform.h"

#include "EventManager_Platform.h"
#include "EventManager_Win32.h"

#include "InputManager_Platform.h"
#include "InputManager_Win32.h"

#include "Buffer_DX12.h"
#include "Buffer_OpenGL.h"
#include "Buffer_Platform.h"

#include "BufferAllocator_DX12.h"
#include "BufferAllocator_OpenGL.h"
#include "BufferAllocator_Platform.h"

#include "RenderManager_DX12.h"
#include "RenderManager_OpenGL.h"
#include "RenderManager_Platform.h"

#include "Sampler_DX12.h"
#include "Sampler_OpenGL.h"
#include "Sampler_Platform.h"

#include "Shader_DX12.h"
#include "Shader_OpenGL.h"
#include "Shader_Platform.h"

#include "SwapChain_DX12.h"
#include "SwapChain_OpenGL.h"
#include "SwapChain_Platform.h"

#include "SysInfo_DX12.h"
#include "SysInfo_OpenGL.h"
#include "SysInfo_Platform.h"

#include "Texture_DX12.h"
#include "Texture_OpenGL.h"
#include "Texture_Platform.h"

#include "TextureTarget_DX12.h"
#include "TextureTarget_OpenGL.h"
#include "TextureTarget_Platform.h"

#include "VertexStream_DX12.h"
#include "VertexStream_OpenGL.h"
#include "VertexStream_Platform.h"

#include "Window_Platform.h"
#include "Window_Win32.h"


namespace platform
{
	// Bind API-specific strategy implementations:
	bool RegisterPlatformFunctions()
	{
		const platform::RenderingAPI& api = en::Config::Get()->GetRenderingAPI();

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

			// Buffer allocator:
			platform::BufferAllocator::Create		= &opengl::BufferAllocator::Create;
			platform::BufferAllocator::Destroy		= &opengl::BufferAllocator::Destroy;

			// Render manager:
			platform::RenderManager::Initialize				= &opengl::RenderManager::Initialize;
			platform::RenderManager::Shutdown				= &opengl::RenderManager::Shutdown;
			platform::RenderManager::CreateAPIResources		= &opengl::RenderManager::CreateAPIResources;
			platform::RenderManager::GetNumFramesInFlight	= &opengl::RenderManager::GetNumFramesInFlight;
			platform::RenderManager::StartImGuiFrame		= &opengl::RenderManager::StartImGuiFrame;
			platform::RenderManager::RenderImGui			= &opengl::RenderManager::RenderImGui;

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

			// Buffer allocator:
			platform::BufferAllocator::Create		= &dx12::BufferAllocator::Create;
			platform::BufferAllocator::Destroy		= &dx12::BufferAllocator::Destroy;
			
			// Render manager:
			platform::RenderManager::Initialize			= &dx12::RenderManager::Initialize;
			platform::RenderManager::Shutdown			= &dx12::RenderManager::Shutdown;
			platform::RenderManager::CreateAPIResources = &dx12::RenderManager::CreateAPIResources;
			platform::RenderManager::GetNumFramesInFlight		= &dx12::RenderManager::GetNumFramesInFlight;
			platform::RenderManager::StartImGuiFrame	= &dx12::RenderManager::StartImGuiFrame;
			platform::RenderManager::RenderImGui		= &dx12::RenderManager::RenderImGui;

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

		LOG("Done!");

		return result;
	}
}
