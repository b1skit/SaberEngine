// © 2022 Adam Badke. All rights reserved.
#include "Core/Assert.h"
#include "Private/RenderManager.h"
#include "Private/TextureTarget_Platform.h"
#include "Private/TextureTarget.h"
#include "Private/TextureTarget_OpenGL.h"
#include "Private/TextureTarget_DX12.h"


namespace platform
{
	void TextureTarget::CreatePlatformObject(re::TextureTarget& texTarget)
	{
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			texTarget.SetPlatformObject(std::make_unique<opengl::TextureTarget::PlatObj>());
		}
		break;
		case RenderingAPI::DX12:
		{
			texTarget.SetPlatformObject(std::make_unique<dx12::TextureTarget::PlatObj>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	void TextureTargetSet::CreatePlatformObject(re::TextureTargetSet& texTarget)
	{
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			texTarget.SetPlatformObject(std::make_unique<opengl::TextureTargetSet::PlatObj>());
		}
		break;
		case RenderingAPI::DX12:
		{
			texTarget.SetPlatformObject(std::make_unique<dx12::TextureTargetSet::PlatObj>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}
}