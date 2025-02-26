// © 2022 Adam Badke. All rights reserved.
#include "Platform.h"

#include "Renderer/AccelerationStructure_DX12.h"
#include "Renderer/AccelerationStructure_Platform.h"

#include "Renderer/Buffer_DX12.h"
#include "Renderer/Buffer_OpenGL.h"
#include "Renderer/Buffer_Platform.h"

#include "Renderer/Context_DX12.h"
#include "Renderer/Context_OpenGL.h"
#include "Renderer/Context_Platform.h"

#include "Renderer/GPUTimer_DX12.h"
#include "Renderer/GPUTimer_OpenGL.h"
#include "Renderer/GPUTimer_Platform.h"

#include "Renderer/RenderManager_DX12.h"
#include "Renderer/RenderManager_OpenGL.h"
#include "Renderer/RenderManager_Platform.h"

#include "Renderer/RLibrary_Platform.h"

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

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/EventManager_Platform.h"
#include "Core/EventManager_Win32.h"
#include "Core/InputManager_Platform.h"
#include "Core/InputManager_Win32.h"

#include "Core/Host/Dialog_Platform.h"
#include "Core/Host/Dialog_Win32.h"
#include "Core/Host/PerformanceTimer_Platform.h"
#include "Core/Host/PerformanceTimer_Win32.h"
#include "Core/Host/Window_Platform.h"
#include "Core/Host/Window_Win32.h"


namespace platform
{
	constexpr char const* RenderingAPIToCStr(platform::RenderingAPI renderingAPI)
	{
		switch (renderingAPI)
		{
		case platform::RenderingAPI::OpenGL: return "OpenGL";
		case platform::RenderingAPI::DX12: return "DX12";
		default: return "platform::RenderingAPIToCStr: Invalid platform::RenderingAPI received";
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

		// Performance timer:
		platform::PerformanceTimer::Create	= &win32::PerformanceTimer::Create;
		platform::PerformanceTimer::Start	= &win32::PerformanceTimer::Start;
		platform::PerformanceTimer::PeekMs	= &win32::PerformanceTimer::PeekMs;
		platform::PerformanceTimer::PeekSec = &win32::PerformanceTimer::PeekSec;

		// Windows dialogues:
		platform::Dialog::OpenFileDialogBox		= &win32::Dialog::OpenFileDialogBox;

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

			// GPU Timer:
			platform::GPUTimer::Create		= &opengl::GPUTimer::Create;
			platform::GPUTimer::BeginFrame	= &opengl::GPUTimer::BeginFrame;
			platform::GPUTimer::EndFrame	= &opengl::GPUTimer::EndFrame;
			platform::GPUTimer::StartTimer	= &opengl::GPUTimer::StartTimer;
			platform::GPUTimer::StopTimer	= &opengl::GPUTimer::StopTimer;

			// Render manager:
			platform::RenderManager::Initialize				= &opengl::RenderManager::Initialize;
			platform::RenderManager::Shutdown				= &opengl::RenderManager::Shutdown;
			platform::RenderManager::CreateAPIResources		= &opengl::RenderManager::CreateAPIResources;
			platform::RenderManager::BeginFrame				= &opengl::RenderManager::BeginFrame;
			platform::RenderManager::EndFrame				= &opengl::RenderManager::EndFrame;
			platform::RenderManager::GetNumFramesInFlight	= &opengl::RenderManager::GetNumFramesInFlight;

			// Shader:
			platform::Shader::Create	= &opengl::Shader::Create;
			platform::Shader::Destroy	= &opengl::Shader::Destroy;

			// SysInfo:
			platform::SysInfo::GetMaxRenderTargets		= &opengl::SysInfo::GetMaxRenderTargets;
			platform::SysInfo::GetMaxTextureBindPoints	= &opengl::SysInfo::GetMaxTextureBindPoints;
			platform::SysInfo::GetMaxVertexAttributes	= &opengl::SysInfo::GetMaxVertexAttributes;

			// Swap chain:
			platform::SwapChain::Create					= &opengl::SwapChain::Create;
			platform::SwapChain::Destroy				= &opengl::SwapChain::Destroy;
			platform::SwapChain::ToggleVSync			= &opengl::SwapChain::ToggleVSync;
			platform::SwapChain::GetBackBufferTargetSet = &opengl::SwapChain::GetBackBufferTargetSet;

			// Texture:
			platform::Texture::Destroy			= &opengl::Texture::Destroy;
			platform::Texture::ShowImGuiWindow	= &opengl::Texture::ShowImGuiWindow;

			// Texture Samplers:
			platform::Sampler::Create	= &opengl::Sampler::Create;
			platform::Sampler::Destroy	= &opengl::Sampler::Destroy;
		}
		break;
		case RenderingAPI::DX12:
		{
			// Acceleration Structure:
			platform::AccelerationStructure::Create		= &dx12::AccelerationStructure::Create;
			platform::AccelerationStructure::Destroy	= &dx12::AccelerationStructure::Destroy;

			// Buffers:
			platform::Buffer::Create			= &dx12::Buffer::Create;
			platform::Buffer::Update			= &dx12::Buffer::Update;
			platform::Buffer::Destroy			= &dx12::Buffer::Destroy;
			platform::Buffer::MapCPUReadback	= &dx12::Buffer::MapCPUReadback;
			platform::Buffer::UnmapCPUReadback	= &dx12::Buffer::UnmapCPUReadback;

			// Context:
			platform::Context::Destroy = &dx12::Context::Destroy;

			// GPU Timer:
			platform::GPUTimer::Create		= &dx12::GPUTimer::Create;
			platform::GPUTimer::BeginFrame	= &dx12::GPUTimer::BeginFrame;
			platform::GPUTimer::EndFrame	= &dx12::GPUTimer::EndFrame;
			platform::GPUTimer::StartTimer	= &dx12::GPUTimer::StartTimer;
			platform::GPUTimer::StopTimer	= &dx12::GPUTimer::StopTimer;
			
			// Render manager:
			platform::RenderManager::Initialize				= &dx12::RenderManager::Initialize;
			platform::RenderManager::Shutdown				= &dx12::RenderManager::Shutdown;
			platform::RenderManager::CreateAPIResources		= &dx12::RenderManager::CreateAPIResources;
			platform::RenderManager::BeginFrame				= &dx12::RenderManager::BeginFrame;
			platform::RenderManager::EndFrame				= &dx12::RenderManager::EndFrame;
			platform::RenderManager::GetNumFramesInFlight	= &dx12::RenderManager::GetNumFramesInFlight;

			// Shader:
			platform::Shader::Create	= &dx12::Shader::Create;
			platform::Shader::Destroy	= &dx12::Shader::Destroy;

			// SysInfo:
			platform::SysInfo::GetMaxRenderTargets		= &dx12::SysInfo::GetMaxRenderTargets;
			platform::SysInfo::GetMaxTextureBindPoints	= &dx12::SysInfo::GetMaxTextureBindPoints;
			platform::SysInfo::GetMaxVertexAttributes	= &dx12::SysInfo::GetMaxVertexAttributes;

			// Swap chain:
			platform::SwapChain::Create					= &dx12::SwapChain::Create;
			platform::SwapChain::Destroy				= &dx12::SwapChain::Destroy;
			platform::SwapChain::ToggleVSync			= &dx12::SwapChain::ToggleVSync;
			platform::SwapChain::GetBackBufferTargetSet = &dx12::SwapChain::GetBackBufferTargetSet;

			// Texture:
			platform::Texture::Destroy			= &dx12::Texture::Destroy;
			platform::Texture::ShowImGuiWindow	= &dx12::Texture::ShowImGuiWindow;

			// Texture Samplers:
			platform::Sampler::Create	= &dx12::Sampler::Create;
			platform::Sampler::Destroy	= &dx12::Sampler::Destroy;
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
