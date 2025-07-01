// © 2022 Adam Badke. All rights reserved.
#include "Core/Assert.h"
#include "RenderManager.h"
#include "TextureTarget_Platform.h"
#include "TextureTarget.h"
#include "TextureTarget_OpenGL.h"
#include "TextureTarget_DX12.h"


namespace platform
{
	void TextureTarget::CreatePlatformObject(re::TextureTarget& texTarget)
	{
		const platform::RenderingAPI api = gr::RenderManager::Get()->GetRenderingAPI();

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
		const platform::RenderingAPI api = gr::RenderManager::Get()->GetRenderingAPI();

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