#include <memory>

#include "DebugConfiguration.h"
#include "Config.h"
#include "TextureTarget_Platform.h"
#include "TextureTarget.h"
#include "TextureTarget_OpenGL.h"

using en::Config;


namespace platform
{
	void TextureTarget::PlatformParams::CreatePlatformParams(gr::TextureTarget& texTarget)
	{
		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			texTarget.m_platformParams = std::make_shared<opengl::TextureTarget::PlatformParams>();
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

		return;
	}


	void TextureTargetSet::PlatformParams::CreatePlatformParams(gr::TextureTargetSet& texTarget)
	{
		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			texTarget.m_platformParams = std::make_shared<opengl::TextureTargetSet::PlatformParams>();
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

		return;
	}


	// platform::TextureTarget static members:
	/****************************************/
	void (*TextureTargetSet::CreateColorTargets)(gr::TextureTargetSet&);
	void (*TextureTargetSet::AttachColorTargets)(gr::TextureTargetSet const& targetSet, uint32_t face, uint32_t mipLevel, bool doBind);

	void (*TextureTargetSet::CreateDepthStencilTarget)(gr::TextureTargetSet& targetSet);
	void (*TextureTargetSet::AttachDepthStencilTarget)(gr::TextureTargetSet const& targetSet, bool doBind);

	uint32_t(*TextureTargetSet::MaxColorTargets)();

}