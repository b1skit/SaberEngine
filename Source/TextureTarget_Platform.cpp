// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Config.h"
#include "TextureTarget_Platform.h"
#include "TextureTarget.h"
#include "TextureTarget_OpenGL.h"

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
			SEAssertF("DX12 is not yet supported");
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
			SEAssertF("DX12 is not yet supported");
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	// platform::TextureTarget static members:
	/****************************************/
	void (*TextureTargetSet::CreateColorTargets)(re::TextureTargetSet&);
	void (*TextureTargetSet::AttachColorTargets)(re::TextureTargetSet& targetSet, uint32_t face, uint32_t mipLevel);

	void (*TextureTargetSet::CreateDepthStencilTarget)(re::TextureTargetSet& targetSet);
	void (*TextureTargetSet::AttachDepthStencilTarget)(re::TextureTargetSet& targetSet);

	uint32_t(*TextureTargetSet::MaxColorTargets)();

}