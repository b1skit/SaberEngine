// © 2022 Adam Badke. All rights reserved.
#include "Platform.h"
#include "Config.h"
#include "DebugConfiguration.h"

#include "Context_Platform.h"
#include "Context_OpenGL.h"
#include "Context_DX12.h"

#include "EventManager_Platform.h"
#include "EventManager_Win32.h"

#include "InputManager_Platform.h"
#include "InputManager_Win32.h"

#include "MeshPrimitive_Platform.h"
#include "MeshPrimitive_OpenGL.h"

#include "ParameterBlock.h"
#include "ParameterBlock_OpenGL.h"

#include "RenderManager_Platform.h"
#include "RenderManager_OpenGL.h"
#include "RenderManager_DX12.h"

#include "Sampler_Platform.h"
#include "Sampler_OpenGL.h"

#include "Shader_Platform.h"
#include "Shader_OpenGL.h"

#include "Texture_Platform.h"
#include "Texture_OpenGL.h"

#include "TextureTarget_Platform.h"
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
			platform::Context::SetVSyncMode			= &opengl::Context::SetVSyncMode;
			platform::Context::SetPipelineState		= &opengl::Context::SetPipelineState;
			platform::Context::GetMaxTextureInputs	= &opengl::Context::GetMaxTextureInputs;
			
			// Render manager:
			platform::RenderManager::Initialize		= &opengl::RenderManager::Initialize;
			platform::RenderManager::Render			= &opengl::RenderManager::Render;
			platform::RenderManager::RenderImGui	= &opengl::RenderManager::RenderImGui;
			platform::RenderManager::Shutdown		= &opengl::RenderManager::Shutdown;

			// MeshPrimitive:
			platform::MeshPrimitive::Create		= &opengl::MeshPrimitive::Create;
			platform::MeshPrimitive::Destroy	= &opengl::MeshPrimitive::Destroy;
			platform::MeshPrimitive::Bind		= &opengl::MeshPrimitive::Bind;

			// Texture:
			platform::Texture::Create			= &opengl::Texture::Create;
			platform::Texture::Destroy			= &opengl::Texture::Destroy;
			platform::Texture::Bind				= &opengl::Texture::Bind;
			platform::Texture::GenerateMipMaps	= &opengl::Texture::GenerateMipMaps;
			platform::Texture::GetUVOrigin		= &opengl::Texture::GetUVOrigin;

			// Texture Samplers:
			platform::Sampler::Create	= &opengl::Sampler::Create;
			platform::Sampler::Destroy	= &opengl::Sampler::Destroy;
			platform::Sampler::Bind		= &opengl::Sampler::Bind;

			// Texture target set:
			platform::TextureTargetSet::CreateColorTargets			= &opengl::TextureTargetSet::CreateColorTargets;
			platform::TextureTargetSet::AttachColorTargets			= &opengl::TextureTargetSet::AttachColorTargets;
			platform::TextureTargetSet::CreateDepthStencilTarget	= &opengl::TextureTargetSet::CreateDepthStencilTarget;
			platform::TextureTargetSet::AttachDepthStencilTarget	= &opengl::TextureTargetSet::AttachDepthStencilTarget;
			platform::TextureTargetSet::MaxColorTargets				= &opengl::TextureTargetSet::MaxColorTargets;

			// Shader:
			platform::Shader::Create			= &opengl::Shader::Create;
			platform::Shader::Bind				= &opengl::Shader::Bind;
			platform::Shader::SetUniform		= &opengl::Shader::SetUniform;
			platform::Shader::SetParameterBlock = &opengl::Shader::SetParameterBlock;
			platform::Shader::Destroy			= &opengl::Shader::Destroy;
			platform::Shader::LoadShaderTexts	= &opengl::Shader::LoadShaderTexts;

			// Parameter blocks:
			platform::ParameterBlock::Create	= &opengl::ParameterBlock::Create;
			platform::ParameterBlock::Update	= &opengl::ParameterBlock::Update;
			platform::ParameterBlock::Destroy	= &opengl::ParameterBlock::Destroy;
			
			result = true;
		}
		break;
		case RenderingAPI::DX12:
		{
			// Context:
			platform::Context::Create				= &dx12::Context::Create;
			platform::Context::Destroy				= &dx12::Context::Destroy;
			platform::Context::Present				= &dx12::Context::Present;
			platform::Context::SetVSyncMode			= &dx12::Context::SetVSyncMode;
			platform::Context::GetMaxTextureInputs	= &dx12::Context::GetMaxTextureInputs;
			
			// Render manager:
			platform::RenderManager::Initialize		= &dx12::RenderManager::Initialize;
			platform::RenderManager::Render			= &dx12::RenderManager::Render;
			platform::RenderManager::RenderImGui	= &dx12::RenderManager::RenderImGui;
			platform::RenderManager::Shutdown		= &dx12::RenderManager::Shutdown;

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
