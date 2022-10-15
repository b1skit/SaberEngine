#include "ParameterBlock_Platform.h"
#include "ParameterBlock.h"
#include "ParameterBlock_OpenGL.h"
#include "DebugConfiguration.h"
#include "CoreEngine.h"


namespace platform
{
	void platform::ParameterBlock::PlatformParams::CreatePlatformParams(re::ParameterBlock& paramBlock)
	{
		SEAssert("Attempting to create platform params for a texture that already exists",
			paramBlock.m_platformParams == nullptr);

		const platform::RenderingAPI& api =
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			paramBlock.m_platformParams = std::make_unique<opengl::ParameterBlock::PlatformParams>();
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

	// Function handles:
	void (*platform::ParameterBlock::Create)(re::ParameterBlock&) = nullptr;
	void (*platform::ParameterBlock::Update)(re::ParameterBlock&) = nullptr;
	void (*platform::ParameterBlock::Destroy)(re::ParameterBlock&) = nullptr;
}