#include <string>

#include "Platform.h"
#include "CoreEngine.h"
#include "BuildConfiguration.h"

#include "Context.h"
#include "Context_Platform.h"
#include "Context_OpenGL.h"

#include "Mesh.h"
#include "Mesh_Platform.h"
#include "Mesh_OpenGL.h"

#include "Texture.h"
#include "Texture_Platform.h"
#include "Texture_OpenGL.h"

#include "TextureTarget_Platform.h"
#include "TextureTarget_OpenGL.h"


namespace platform
{
	// Bind API-specific strategy implementations:
	bool RegisterPlatformFunctions()
	{
		LOG("Configuring API-specific platform bindings...");

		const platform::RenderingAPI& api = 
			SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();

		bool result = false;
		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			// API-specific:
			platform::Context::Create			= &opengl::Context::Create;
			platform::Context::Destroy			= &opengl::Context::Destroy;
			platform::Context::SwapWindow		= &opengl::Context::SwapWindow;
			platform::Context::SetCullingMode	= &opengl::Context::SetCullingMode;
			platform::Context::ClearTargets		= &opengl::Context::ClearTargets;

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

			// Texture target:
			

			// Texture target set:
			platform::TextureTargetSet::CreateColorTargets	= &opengl::TextureTargetSet::CreateColorTargets;
			platform::TextureTargetSet::AttachColorTargets	= &opengl::TextureTargetSet::AttachColorTargets;

			platform::TextureTargetSet::CreateDepthStencilTarget = &opengl::TextureTargetSet::CreateDepthStencilTarget;
			platform::TextureTargetSet::AttachDepthStencilTarget = &opengl::TextureTargetSet::AttachDepthStencilTarget;

			platform::TextureTargetSet::MaxColorTargets		= &opengl::TextureTargetSet::MaxColorTargets;

			result = true;
		}
		break;

		case RenderingAPI::DX12:
		{
			assert("Unsupported rendering API" && false);
			result = false;
		}
		break;

		default:
		{
			assert("Unsupported rendering API" && false);
			result = false;
		}
		}

		LOG("Done!");

		return result;
	}
}
