#include "ParameterBlock_Platform.h"
#include "ParameterBlock.h"
#include "ParameterBlock_OpenGL.h"
#include "DebugConfiguration.h"
#include "CoreEngine.h"


namespace platform
{
	// Parameter struct object factory:
	void platform::PermanentParameterBlock::PlatformParams::CreatePlatformParams(re::PermanentParameterBlock& paramBlock)
	{
		SEAssert("Attempting to create platform params for a texture that already exists",
			paramBlock.m_platformParams == nullptr);

		const platform::RenderingAPI& api =
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			paramBlock.m_platformParams = std::make_unique<opengl::PermanentParameterBlock::PlatformParams>();
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
	void (*platform::PermanentParameterBlock::Create)(re::PermanentParameterBlock&) = nullptr;
	void (*platform::PermanentParameterBlock::Destroy)(re::PermanentParameterBlock&) = nullptr;
}