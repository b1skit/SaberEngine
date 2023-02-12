// � 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "VertexStream_OpenGL.h"
#include "VertexStream_Platform.h"


namespace platform
{
	std::unique_ptr<re::VertexStream::PlatformParams> VertexStream::CreatePlatformParams()
	{
		const platform::RenderingAPI& api = en::Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			return std::make_unique<opengl::VertexStream::PlatformParams>();
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
		return nullptr;
	}
}