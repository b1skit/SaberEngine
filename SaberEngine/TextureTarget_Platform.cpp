#include <memory>

#include "DebugConfiguration.h"
#include "CoreEngine.h"
#include "TextureTarget_Platform.h"
#include "TextureTarget.h"
#include "TextureTarget_OpenGL.h"


namespace platform
{
	void TextureTarget::PlatformParams::CreatePlatformParams(gr::TextureTarget& texTarget)
	{
		const platform::RenderingAPI& api =
			SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			texTarget.m_platformParams = std::make_shared<opengl::TextureTarget::PlatformParams>();
		}
		break;
		case RenderingAPI::DX12:
		{
			SEAssert("DX12 is not yet supported", false);
		}
		break;
		default:
		{
			SEAssert("Invalid rendering API argument received", false);
		}
		}

		return;
	}


	void TextureTargetSet::PlatformParams::CreatePlatformParams(gr::TextureTargetSet& texTarget)
	{
		const platform::RenderingAPI& api =
			SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			texTarget.m_platformParams = std::make_shared<opengl::TextureTargetSet::PlatformParams>();
		}
		break;
		case RenderingAPI::DX12:
		{
			SEAssert("DX12 is not yet supported", false);
		}
		break;
		default:
		{
			SEAssert("Invalid rendering API argument received", false);
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