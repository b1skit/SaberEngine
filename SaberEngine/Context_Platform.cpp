#include "CoreEngine.h"
#include "Context.h"
#include "Context_OpenGL.h"
#include "Context_Platform.h"



namespace platform
{
	void Context::PlatformParams::CreatePlatformParams(re::Context& context)
	{
		const platform::RenderingAPI& api =
			SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			context.m_platformParams =
				std::make_unique<opengl::Context::PlatformParams>();
		}
		break;
		case RenderingAPI::DX12:
		{
			assert("DX12 is not yet supported" && false);
		}
		break;
		default:
		{
			assert("Invalid rendering API argument received" && false);
		}
		}

		return;
	}

	
	void (*platform::Context::Create)(re::Context& context);
	void (*platform::Context::Destroy)(re::Context& context);
	void (*platform::Context::SwapWindow)(re::Context& context);
}