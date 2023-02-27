// � 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Config.h"
#include "TextureTarget_Platform.h"
#include "TextureTarget.h"
#include "TextureTarget_OpenGL.h"
#include "TextureTarget_DX12.h"


using en::Config;


namespace platform
{
	void TextureTarget::CreatePlatformParams(re::TextureTarget& texTarget)
	{
		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			texTarget.SetPlatformParams(std::make_shared<opengl::TextureTarget::PlatformParams>());
		}
		break;
		case RenderingAPI::DX12:
		{
			texTarget.SetPlatformParams(std::make_shared<dx12::TextureTarget::PlatformParams>());
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
		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			texTarget.SetPlatformParams(std::make_shared<opengl::TextureTargetSet::PlatformParams>());
		}
		break;
		case RenderingAPI::DX12:
		{
			texTarget.SetPlatformParams(std::make_shared<dx12::TextureTargetSet::PlatformParams>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}
}