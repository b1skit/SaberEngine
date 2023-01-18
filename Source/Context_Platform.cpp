// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Config.h"
#include "Context.h"
#include "Context_Platform.h"
#include "Context_OpenGL.h"


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
			SEAssertF("DX12 is not yet supported");
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
	void (*platform::Context::SetVSyncMode)(re::Context const& window, bool enabled) = nullptr;
	void (*platform::Context::SetCullingMode)(re::Context::FaceCullingMode const& mode) = nullptr;
	void (*platform::Context::ClearTargets)(re::Context::ClearTarget const& clearTarget) = nullptr;
	void (*platform::Context::SetBlendMode)(re::Context::BlendMode const& src, re::Context::BlendMode const& dst) = nullptr;
	void (*platform::Context::SetDepthTestMode)(re::Context::DepthTestMode const& mode) = nullptr;
	void (*platform::Context::SetDepthWriteMode)(re::Context::DepthWriteMode const& mode) = nullptr;
	void (*platform::Context::SetColorWriteMode)(re::Context::ColorWriteMode const& channelModes) = nullptr;
	uint32_t(*platform::Context::GetMaxTextureInputs)() = nullptr;
}