#include <string>

#include "Platform.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"

#include "Context_Platform.h"
#include "Context_OpenGL.h"

#include "RenderManager_Platform.h"
#include "RenderManager_OpenGL.h"

#include "Mesh_Platform.h"
#include "Mesh_OpenGL.h"

#include "Texture_Platform.h"
#include "Texture_OpenGL.h"

#include "Sampler_Platform.h"
#include "Sampler_OpenGL.h"

#include "TextureTarget_Platform.h"
#include "TextureTarget_OpenGL.h"

#include "Shader_Platform.h"
#include "Shader_OpenGL.h"

#include "ParameterBlock.h"
#include "ParameterBlock_OpenGL.h"


namespace platform
{
	// Bind API-specific strategy implementations:
	bool RegisterPlatformFunctions()
	{
		LOG("Configuring API-specific platform bindings...");

		const platform::RenderingAPI& api = 
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();

		bool result = false;
		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			// API-specific:
			platform::Context::Create				= &opengl::Context::Create;
			platform::Context::Destroy				= &opengl::Context::Destroy;
			platform::Context::SwapWindow			= &opengl::Context::SwapWindow;
			platform::Context::SetCullingMode		= &opengl::Context::SetCullingMode;
			platform::Context::ClearTargets			= &opengl::Context::ClearTargets;
			platform::Context::SetBlendMode			= &opengl::Context::SetBlendMode;
			platform::Context::SetDepthTestMode		= &opengl::Context::SetDepthTestMode;
			platform::Context::SetDepthWriteMode	= &opengl::Context::SetDepthWriteMode;
			platform::Context::SetColorWriteMode	= &opengl::Context::SetColorWriteMode;
			platform::Context::GetMaxTextureInputs	= &opengl::Context::GetMaxTextureInputs;

			// Render manager:
			platform::RenderManager::Initialize		= &opengl::RenderManager::Initialize;
			platform::RenderManager::Render			= &opengl::RenderManager::Render;
			
			// Mesh:
			platform::Mesh::Create	= &opengl::Mesh::Create;
			platform::Mesh::Destroy	= &opengl::Mesh::Destroy;
			platform::Mesh::Bind	= &opengl::Mesh::Bind;

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


			// Texture target:
			
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

			// Parameter blocks:
			platform::PermanentParameterBlock::Create	= &opengl::PermanentParameterBlock::Create;
			platform::PermanentParameterBlock::Destroy	= &opengl::PermanentParameterBlock::Destroy;


			result = true;
		}
		break;

		case RenderingAPI::DX12:
		{
			SEAssert("Unsupported rendering API", false);
			result = false;
		}
		break;

		default:
		{
			SEAssert("Unsupported rendering API", false);
			result = false;
		}
		}

		LOG("Done!");

		return result;
	}
}
