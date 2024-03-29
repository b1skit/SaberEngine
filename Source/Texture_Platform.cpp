// � 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Config.h"
#include "Texture.h"
#include "Texture_Platform.h"
#include "Texture_OpenGL.h"
#include "Texture_DX12.h"


namespace platform
{
	void platform::Texture::CreatePlatformParams(re::Texture& texture)
	{
		const platform::RenderingAPI& api = en::Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			texture.SetPlatformParams(std::make_unique<opengl::Texture::PlatformParams>(texture));
		}
		break;
		case RenderingAPI::DX12:
		{
			texture.SetPlatformParams(std::make_unique<dx12::Texture::PlatformParams>(texture));
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
}