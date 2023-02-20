// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Config.h"
#include "MeshPrimitive_DX12.h"
#include "MeshPrimitive_OpenGL.h"
#include "MeshPrimitive_Platform.h"

using en::Config;


namespace platform
{
	void platform::MeshPrimitive::CreatePlatformParams(re::MeshPrimitive& meshPrimitive)
	{
		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			meshPrimitive.SetPlatformParams(std::make_unique<opengl::MeshPrimitive::PlatformParams>(meshPrimitive));
		}
		break;
		case RenderingAPI::DX12:
		{
			meshPrimitive.SetPlatformParams(std::make_unique<dx12::MeshPrimitive::PlatformParams>(meshPrimitive));
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	void (*MeshPrimitive::Destroy)(re::MeshPrimitive& meshPrimitive) = nullptr;
}