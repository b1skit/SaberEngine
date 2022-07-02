#include <string>

#include "CoreEngine.h"
#include "rePlatform.h"
#include "BuildConfiguration.h"

#include "grMesh.h"
#include "reMesh_Platform.h"
#include "reMesh_OpenGL.h"


namespace re::platform
{
	// Bind API-specific strategy implementations:
	bool RegisterPlatformFunctions()
	{
		const re::platform::RenderingAPI& api = 
			SaberEngine::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			gr::Mesh::Create = &re::platform::opengl::Create;
			gr::Mesh::Delete = &re::platform::opengl::Delete;
			gr::Mesh::Bind = &re::platform::opengl::Bind;

			return true;
		}

		case RenderingAPI::DX12:
		{
			// TODO: Assert, log an error, etc
			return false;
		}

		default:
		{
			// TODO: Assert, log an error, etc
			return false;
		}
		}

		return false;
	}
}
