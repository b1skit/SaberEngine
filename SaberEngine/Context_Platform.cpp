#include "DebugConfiguration.h"
#include "CoreEngine.h"
#include "Context.h"
#include "Context_OpenGL.h"
#include "Context_Platform.h"


namespace platform
{
	void Context::PlatformParams::CreatePlatformParams(re::Context& context)
	{
		const platform::RenderingAPI& api =
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			context.m_platformParams = std::make_unique<opengl::Context::PlatformParams>();
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

	
	void (*platform::Context::Create)(re::Context& context);
	void (*platform::Context::Destroy)(re::Context& context);
	void (*platform::Context::SwapWindow)(re::Context const& context);
	void (*platform::Context::SetCullingMode)(platform::Context::FaceCullingMode const& mode);
	void (*platform::Context::ClearTargets)(platform::Context::ClearTarget const& clearTarget);
	void (*platform::Context::SetBlendMode)(platform::Context::BlendMode const& src, platform::Context::BlendMode const& dst);
	void (*platform::Context::SetDepthTestMode)(DepthTestMode const& mode);
	void (*platform::Context::SetDepthWriteMode)(DepthWriteMode const& mode);
	void (*platform::Context::SetColorWriteMode)(ColorWriteMode const& channelModes);
	uint32_t(*platform::Context::GetMaxTextureInputs)();
}