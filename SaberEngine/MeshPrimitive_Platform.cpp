#include "DebugConfiguration.h"
#include "CoreEngine.h"
#include "MeshPrimitive_Platform.h"
#include "MeshPrimitive_OpenGL.h"



namespace platform
{
	void platform::MeshPrimitive::PlatformParams::CreatePlatformParams(re::MeshPrimitive& meshPrimitive)
	{
		const platform::RenderingAPI& api =
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			meshPrimitive.GetPlatformParams() = std::make_unique<opengl::MeshPrimitive::PlatformParams>(meshPrimitive);
		}
		break;
		case RenderingAPI::DX12:
		{
			SEAssertF("DX12 is not yet supported");
			return;
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
			return;
		}
		}
	}

	// platform::MeshPrimitive static members:
	/********************************/
	void (*MeshPrimitive::Create)(re::MeshPrimitive& meshPrimitive);
	void (*MeshPrimitive::Bind)(platform::MeshPrimitive::PlatformParams const* params, bool doBind);
	void (*MeshPrimitive::Destroy)(re::MeshPrimitive& meshPrimitive);
}