#include "DebugConfiguration.h"
#include "Config.h"
#include "Texture.h"
#include "Texture_Platform.h"
#include "Texture_OpenGL.h"

using en::Config;


namespace platform
{
	// Parameter struct object factory:
	void platform::Texture::PlatformParams::CreatePlatformParams(gr::Texture& texture)
	{
		SEAssert("Attempting to create platform params for a texture that already exists", 
			texture.m_platformParams == nullptr);

		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			texture.m_platformParams = std::make_unique<opengl::Texture::PlatformParams>(
				texture.GetTextureParams());
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
	void (*platform::Texture::Create)(gr::Texture&);
	void (*platform::Texture::Bind)(gr::Texture const&, uint32_t textureUnit, bool doBind);
	void (*platform::Texture::Destroy)(gr::Texture&);
	void (*platform::Texture::GenerateMipMaps)(gr::Texture&);

	platform::Texture::UVOrigin (*platform::Texture::GetUVOrigin)();
}