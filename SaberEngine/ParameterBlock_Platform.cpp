#include "ParameterBlock_Platform.h"
#include "ParameterBlock.h"
#include "ParameterBlock_OpenGL.h"
#include "DebugConfiguration.h"
#include "Config.h"

using en::Config;


namespace platform
{
	void platform::ParameterBlock::PlatformParams::CreatePlatformParams(re::ParameterBlock& paramBlock)
	{
		SEAssert("Attempting to create platform params for a texture that already exists",
			paramBlock.m_platformParams == nullptr);

		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			paramBlock.m_platformParams = std::make_unique<opengl::ParameterBlock::PlatformParams>();
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

	// Function handles:
	void (*platform::ParameterBlock::Create)(re::ParameterBlock&) = nullptr;
	void (*platform::ParameterBlock::Update)(re::ParameterBlock&) = nullptr;
	void (*platform::ParameterBlock::Destroy)(re::ParameterBlock&) = nullptr;
}