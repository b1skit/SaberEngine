// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "DebugConfiguration.h"
#include "Platform.h"

#include "Context_DX12.h"
#include "Context_OpenGL.h"
#include "Context_Platform.h"

#include "EventManager_Platform.h"
#include "EventManager_Win32.h"

#include "InputManager_Platform.h"
#include "InputManager_Win32.h"

#include "ParameterBlock.h"
#include "ParameterBlock_DX12.h"
#include "ParameterBlock_OpenGL.h"

#include "RenderManager_Platform.h"
#include "RenderManager_DX12.h"
#include "RenderManager_OpenGL.h"

#include "Sampler_DX12.h"
#include "Sampler_OpenGL.h"
#include "Sampler_Platform.h"

#include "Shader_DX12.h"
#include "Shader_Platform.h"
#include "Shader_OpenGL.h"

#include "SwapChain_DX12.h"
#include "SwapChain_Platform.h"
#include "SwapChain_OpenGL.h"

#include "SysInfo_DX12.h"
#include "SysInfo_Platform.h"
#include "SysInfo_OpenGL.h"

#include "Texture_DX12.h"
#include "Texture_Platform.h"
#include "Texture_OpenGL.h"

#include "TextureTarget_DX12.h"
#include "TextureTarget_Platform.h"
#include "TextureTarget_OpenGL.h"

#include "VertexStream_DX12.h"
#include "VertexStream_Platform.h"
#include "VertexStream_OpenGL.h"

#include "Window_Platform.h"
#include "Window_Win32.h"

using en::Config;


namespace platform
{
	// Bind API-specific strategy implementations:
	bool RegisterPlatformFunctions()
	{
		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();

		LOG("Configuring API-specific platform bindings...");

		bool result = false;

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

			// Parameter blocks:
			platform::ParameterBlock::Create			= &opengl::ParameterBlock::Create;
			platform::ParameterBlock::Update			= &opengl::ParameterBlock::Update;
			platform::ParameterBlock::Destroy			= &opengl::ParameterBlock::Destroy;

			// Render manager:
			platform::RenderManager::Initialize			= &opengl::RenderManager::Initialize;
			platform::RenderManager::Shutdown			= &opengl::RenderManager::Shutdown;
			platform::RenderManager::CreateAPIResources = &opengl::RenderManager::CreateAPIResources;
			platform::RenderManager::GetNumFrames		= &opengl::RenderManager::GetNumFrames;
			platform::RenderManager::StartImGuiFrame	= &opengl::RenderManager::StartImGuiFrame;
			platform::RenderManager::RenderImGui		= &opengl::RenderManager::RenderImGui;

			// Shader:
			platform::Shader::Create	= &opengl::Shader::Create;
			platform::Shader::Destroy	= &opengl::Shader::Destroy;

			// SysInfo:
			platform::SysInfo::GetMaxRenderTargets = &opengl::SysInfo::GetMaxRenderTargets;

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

			result = true;
		}
		break;
		case RenderingAPI::DX12:
		{
			// Context:
			platform::Context::Destroy	= &dx12::Context::Destroy;

			// Parameter blocks:
			platform::ParameterBlock::Create	= &dx12::ParameterBlock::Create;
			platform::ParameterBlock::Update	= &dx12::ParameterBlock::Update;
			platform::ParameterBlock::Destroy	= &dx12::ParameterBlock::Destroy;
			
			// Render manager:
			platform::RenderManager::Initialize			= &dx12::RenderManager::Initialize;
			platform::RenderManager::Shutdown			= &dx12::RenderManager::Shutdown;
			platform::RenderManager::CreateAPIResources = &dx12::RenderManager::CreateAPIResources;
			platform::RenderManager::GetNumFrames		= &dx12::RenderManager::GetNumFrames;
			platform::RenderManager::StartImGuiFrame	= &dx12::RenderManager::StartImGuiFrame;
			platform::RenderManager::RenderImGui		= &dx12::RenderManager::RenderImGui;

			// Shader:
			platform::Shader::Create	= &dx12::Shader::Create;
			platform::Shader::Destroy	= &dx12::Shader::Destroy;

			// SysInfo:
			platform::SysInfo::GetMaxRenderTargets = &dx12::SysInfo::GetMaxRenderTargets;

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

			result = true;
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
