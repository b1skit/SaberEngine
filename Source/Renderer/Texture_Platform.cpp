// © 2022 Adam Badke. All rights reserved.
#include "RenderManager.h"
#include "Texture.h"
#include "Texture_Platform.h"
#include "Texture_OpenGL.h"
#include "Texture_DX12.h"


namespace platform
{
	void Texture::CreatePlatformObject(re::Texture& texture)
	{
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			texture.SetPlatformObject(std::make_unique<opengl::Texture::PlatObj>(texture));
		}
		break;
		case RenderingAPI::DX12:
		{
			texture.SetPlatformObject(std::make_unique<dx12::Texture::PlatObj>(texture));
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	void Texture::CreateAPIResource(core::InvPtr<re::Texture> const& texture, void* platformObject)
	{
		platform::Texture::Create(texture, platformObject);

		re::Texture::RegisterBindlessResourceHandles(texture);
	}


	// platform::Texture static members:
	/***********************************/
	void (*Texture::Create)(core::InvPtr<re::Texture> const&, void*) = nullptr;
	void (*Texture::Destroy)(re::Texture&) = nullptr;
	void (*Texture::ShowImGuiWindow)(core::InvPtr<re::Texture> const&, float scale) = nullptr;
}