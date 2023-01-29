// � 2022 Adam Badke. All rights reserved.
#include "Platform.h"
#include "Config.h"
#include "DebugConfiguration.h"

#include "Context_Platform.h"
#include "Context_DX12.h"
#include "Context_OpenGL.h"

#include "EventManager_Platform.h"
#include "EventManager_Win32.h"

#include "InputManager_Platform.h"
#include "InputManager_Win32.h"

#include "MeshPrimitive_Platform.h"
#include "MeshPrimitive_OpenGL.h"

#include "ParameterBlock.h"
#include "ParameterBlock_DX12.h"
#include "ParameterBlock_OpenGL.h"

#include "RenderManager_Platform.h"
#include "RenderManager_DX12.h"
#include "RenderManager_OpenGL.h"

#include "Sampler_Platform.h"
#include "Sampler_OpenGL.h"

#include "Shader_Platform.h"
#include "Shader_OpenGL.h"

#include "SwapChain_Platform.h"
#include "SwapChain_DX12.h"
#include "SwapChain_OpenGL.h"

#include "Texture_Platform.h"
#include "Texture_DX12.h"
#include "Texture_OpenGL.h"

#include "TextureTarget_Platform.h"
#include "TextureTarget_DX12.h"
#include "TextureTarget_OpenGL.h"

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
		platform::EventManager::ProcessMessages		= &win32::EventManager::ProcessMessages;

		// Rendering API-specific bindings:
		/*********************************/
		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			// Context:
			platform::Context::Create				= &opengl::Context::Create;
			platform::Context::Destroy				= &opengl::Context::Destroy;
			platform::Context::Present				= &opengl::Context::Present;
			platform::Context::SetPipelineState		= &opengl::Context::SetPipelineState;
			platform::Context::GetMaxTextureInputs	= &opengl::Context::GetMaxTextureInputs;
			platform::Context::GetMaxColorTargets	= &opengl::Context::GetMaxColorTargets;
			
			// MeshPrimitive:
			platform::MeshPrimitive::Create		= &opengl::MeshPrimitive::Create;
			platform::MeshPrimitive::Destroy	= &opengl::MeshPrimitive::Destroy;
			platform::MeshPrimitive::Bind		= &opengl::MeshPrimitive::Bind;

			// Parameter blocks:
			platform::ParameterBlock::Create	= &opengl::ParameterBlock::Create;
			platform::ParameterBlock::Update	= &opengl::ParameterBlock::Update;
			platform::ParameterBlock::Destroy	= &opengl::ParameterBlock::Destroy;

			// Render manager:
			platform::RenderManager::Initialize		= &opengl::RenderManager::Initialize;
			platform::RenderManager::Render			= &opengl::RenderManager::Render;
			platform::RenderManager::RenderImGui	= &opengl::RenderManager::RenderImGui;
			platform::RenderManager::Shutdown		= &opengl::RenderManager::Shutdown;

			// Shader:
			platform::Shader::Create			= &opengl::Shader::Create;
			platform::Shader::Bind				= &opengl::Shader::Bind;
			platform::Shader::SetUniform		= &opengl::Shader::SetUniform;
			platform::Shader::SetParameterBlock = &opengl::Shader::SetParameterBlock;
			platform::Shader::Destroy			= &opengl::Shader::Destroy;
			platform::Shader::LoadShaderTexts	= &opengl::Shader::LoadShaderTexts;

			// Swap chain:
			platform::SwapChain::Create			= &opengl::SwapChain::Create;
			platform::SwapChain::Destroy		= &opengl::SwapChain::Destroy;
			platform::SwapChain::SetVSyncMode	= &opengl::SwapChain::SetVSyncMode;

			// Texture:
			platform::Texture::Create			= &opengl::Texture::Create;
			platform::Texture::Destroy			= &opengl::Texture::Destroy;
			platform::Texture::Bind				= &opengl::Texture::Bind;
			platform::Texture::GenerateMipMaps	= &opengl::Texture::GenerateMipMaps;

			// Texture Samplers:
			platform::Sampler::Create	= &opengl::Sampler::Create;
			platform::Sampler::Destroy	= &opengl::Sampler::Destroy;
			platform::Sampler::Bind		= &opengl::Sampler::Bind;

			// Texture target set:
			platform::TextureTargetSet::CreateColorTargets			= &opengl::TextureTargetSet::CreateColorTargets;
			platform::TextureTargetSet::AttachColorTargets			= &opengl::TextureTargetSet::AttachColorTargets;
			platform::TextureTargetSet::CreateDepthStencilTarget	= &opengl::TextureTargetSet::CreateDepthStencilTarget;
			platform::TextureTargetSet::AttachDepthStencilTarget	= &opengl::TextureTargetSet::AttachDepthStencilTarget;

			result = true;
		}
		break;
		case RenderingAPI::DX12:
		{
			// Context:
			platform::Context::Create				= &dx12::Context::Create;
			platform::Context::Destroy				= &dx12::Context::Destroy;
			platform::Context::Present				= &dx12::Context::Present;
			platform::Context::GetMaxTextureInputs	= &dx12::Context::GetMaxTextureInputs;
			platform::Context::GetMaxColorTargets	= &dx12::Context::GetMaxColorTargets;

			// Parameter blocks:
			platform::ParameterBlock::Create	= &dx12::ParameterBlock::Create;
			platform::ParameterBlock::Update	= &dx12::ParameterBlock::Update;
			platform::ParameterBlock::Destroy	= &dx12::ParameterBlock::Destroy;
			
			// Render manager:
			platform::RenderManager::Initialize		= &dx12::RenderManager::Initialize;
			platform::RenderManager::Render			= &dx12::RenderManager::Render;
			platform::RenderManager::RenderImGui	= &dx12::RenderManager::RenderImGui;
			platform::RenderManager::Shutdown		= &dx12::RenderManager::Shutdown;

			// Swap chain:
			platform::SwapChain::Create			= &dx12::SwapChain::Create;
			platform::SwapChain::Destroy		= &dx12::SwapChain::Destroy;
			platform::SwapChain::SetVSyncMode	= &dx12::SwapChain::SetVSyncMode;

			// Texture:
			platform::Texture::Create			= &dx12::Texture::Create;
			platform::Texture::Destroy			= &dx12::Texture::Destroy;
			platform::Texture::Bind				= &dx12::Texture::Bind;
			platform::Texture::GenerateMipMaps	= &dx12::Texture::GenerateMipMaps;

			// Texture target set:
			platform::TextureTargetSet::CreateColorTargets			= &dx12::TextureTargetSet::CreateColorTargets;
			platform::TextureTargetSet::AttachColorTargets			= &dx12::TextureTargetSet::AttachColorTargets;
			platform::TextureTargetSet::CreateDepthStencilTarget	= &dx12::TextureTargetSet::CreateDepthStencilTarget;
			platform::TextureTargetSet::AttachDepthStencilTarget	= &dx12::TextureTargetSet::AttachDepthStencilTarget;

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
