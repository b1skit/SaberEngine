// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Config.h"
#include "Context.h"
#include "Context_Platform.h"
#include "Context_OpenGL.h"
#include "Context_DX12.h"


namespace platform
{
	using en::Config;


	void Context::CreatePlatformParams(re::Context& context)
	{
		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			context.SetPlatformParams(std::make_unique<opengl::Context::PlatformParams>());
		}
		break;
		case RenderingAPI::DX12:
		{
			context.SetPlatformParams(std::make_unique<dx12::Context::PlatformParams>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}
	
	
	void (*platform::Context::Create)(re::Context& context) = nullptr;
	void (*platform::Context::Destroy)(re::Context& context) = nullptr;
	void (*platform::Context::Present)(re::Context const& context) = nullptr;
	void (*platform::Context::SetPipelineState)(re::Context const& context, gr::PipelineState const& pipelineState) = nullptr;
	uint8_t (*platform::Context::GetMaxTextureInputs)() = nullptr;
	uint8_t (*platform::Context::GetMaxColorTargets)() = nullptr;
}