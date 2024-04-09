// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Config.h"
#include "TextureTarget_Platform.h"
#include "TextureTarget.h"
#include "TextureTarget_OpenGL.h"
#include "TextureTarget_DX12.h"


namespace platform
{
	void TextureTarget::CreatePlatformParams(re::TextureTarget& texTarget)
	{
		const platform::RenderingAPI api = en::Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			texTarget.SetPlatformParams(std::make_unique<opengl::TextureTarget::PlatformParams>());
		}
		break;
		case RenderingAPI::DX12:
		{
			texTarget.SetPlatformParams(std::make_unique<dx12::TextureTarget::PlatformParams>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	void TextureTargetSet::CreatePlatformParams(re::TextureTargetSet& texTarget)
	{
		const platform::RenderingAPI api = en::Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			texTarget.SetPlatformParams(std::make_unique<opengl::TextureTargetSet::PlatformParams>());
		}
		break;
		case RenderingAPI::DX12:
		{
			texTarget.SetPlatformParams(std::make_unique<dx12::TextureTargetSet::PlatformParams>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}
}