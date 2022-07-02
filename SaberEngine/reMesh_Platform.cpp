#include "CoreEngine.h"
#include "reMesh_Platform.h"
#include "reMesh_OpenGL.h"


namespace re::platform
{
	std::unique_ptr<MeshParams_Platform> re::platform::MeshParams_Platform::Create()
	{
		const re::platform::RenderingAPI& api =
			SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			return std::make_unique<re::platform::opengl::MeshParams_OpenGL>();
		}

		case RenderingAPI::DX12:
		{
			assert("DX12 is not yet supported" && false);
			return nullptr;
		}

		default:
		{
			assert("Invalid rendering API argument received" && false);
			return nullptr;
		}
		}

		return nullptr;
	}
}