#include "BuildConfiguration.h"
#include "CoreEngine.h"
#include "Mesh_Platform.h"
#include "Mesh_OpenGL.h"


namespace platform
{
	std::unique_ptr<Mesh::PlatformParams> platform::Mesh::PlatformParams::CreatePlatformParams()
	{
		const platform::RenderingAPI& api =
			SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			return std::make_unique<opengl::Mesh::PlatformParams>();
		}

		case RenderingAPI::DX12:
		{
			SEAssert("DX12 is not yet supported", false);
			return nullptr;
		}

		default:
		{
			SEAssert("Invalid rendering API argument received", false);
			return nullptr;
		}
		}

		return nullptr;
	}

	// platform::Mesh static members:
	/********************************/
	void (*Mesh::Create)(gr::Mesh& mesh);
	void (*Mesh::Bind)(gr::Mesh& mesh, bool doBind);
	void (*Mesh::Destroy)(gr::Mesh& mesh);
}