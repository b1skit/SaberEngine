#include "DebugConfiguration.h"
#include "CoreEngine.h"
#include "Mesh_Platform.h"
#include "Mesh_OpenGL.h"



namespace platform
{
	void platform::Mesh::PlatformParams::CreatePlatformParams(gr::Mesh& mesh)
	{
		const platform::RenderingAPI& api =
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			mesh.GetPlatformParams() = std::make_unique<opengl::Mesh::PlatformParams>(mesh);
		}
		break;
		case RenderingAPI::DX12:
		{
			SEAssert("DX12 is not yet supported", false);
			return;
		}
		break;
		default:
		{
			SEAssert("Invalid rendering API argument received", false);
			return;
		}
		}
	}

	// platform::Mesh static members:
	/********************************/
	void (*Mesh::Create)(gr::Mesh& mesh);
	void (*Mesh::Bind)(gr::Mesh& mesh, bool doBind);
	void (*Mesh::Destroy)(gr::Mesh& mesh);
}