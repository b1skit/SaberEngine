#include <memory>

#include "DebugConfiguration.h"
#include "Config.h"
#include "TextureTarget_Platform.h"
#include "TextureTarget.h"
#include "TextureTarget_OpenGL.h"

using en::Config;


namespace platform
{
	void TextureTarget::PlatformParams::CreatePlatformParams(re::TextureTarget& texTarget)
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
	}


	void TextureTargetSet::PlatformParams::CreatePlatformParams(re::TextureTargetSet& texTarget)
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
	}


	// platform::TextureTarget static members:
	/****************************************/
	void (*TextureTargetSet::CreateColorTargets)(re::TextureTargetSet&);
	void (*TextureTargetSet::AttachColorTargets)(re::TextureTargetSet const& targetSet, uint32_t face, uint32_t mipLevel, bool doBind);

	void (*TextureTargetSet::CreateDepthStencilTarget)(re::TextureTargetSet& targetSet);
	void (*TextureTargetSet::AttachDepthStencilTarget)(re::TextureTargetSet const& targetSet, bool doBind);

	uint32_t(*TextureTargetSet::MaxColorTargets)();

}