#include "DebugConfiguration.h"
#include "Config.h"
#include "Texture.h"
#include "Texture_Platform.h"
#include "Texture_OpenGL.h"

using en::Config;


namespace platform
{
	// Parameter struct object factory:
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
			SEAssertF("DX12 is not yet supported");
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
	void (*platform::Texture::Create)(re::Texture&);
	void (*platform::Texture::Bind)(re::Texture&, uint32_t textureUnit);
	void (*platform::Texture::Destroy)(re::Texture&);
	void (*platform::Texture::GenerateMipMaps)(re::Texture&);

	platform::Texture::UVOrigin (*platform::Texture::GetUVOrigin)();
}