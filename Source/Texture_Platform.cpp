// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Config.h"
#include "Texture.h"
#include "Texture_Platform.h"
#include "Texture_OpenGL.h"
#include "Texture_DX12.h"

using en::Config;


namespace platform
{
	void platform::Texture::CreatePlatformParams(re::Texture& texture)
	{
		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			texture.SetPlatformParams(std::make_unique<opengl::Texture::PlatformParams>(texture.GetTextureParams()));
		}
		break;
		case RenderingAPI::DX12:
		{
			texture.SetPlatformParams(std::make_unique<dx12::Texture::PlatformParams>(texture.GetTextureParams()));
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	// platform::Texture static members:
	/***********************************/
	void (*platform::Texture::Destroy)(re::Texture&) = nullptr;
	void (*platform::Texture::GenerateMipMaps)(re::Texture&) = nullptr;
}