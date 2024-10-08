// © 2022 Adam Badke. All rights reserved.
#include "RenderManager.h"
#include "VertexStream_DX12.h"
#include "VertexStream_OpenGL.h"
#include "VertexStream_Platform.h"

#include "Core/Config.h"


namespace platform
{
	std::unique_ptr<re::VertexStream::PlatformParams> VertexStream::CreatePlatformParams(re::VertexStream& stream)
	{
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			return opengl::VertexStream::CreatePlatformParams(stream);
		}
		break;
		case RenderingAPI::DX12:
		{
			return dx12::VertexStream::CreatePlatformParams(stream);
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	void (*VertexStream::Destroy)(re::VertexStream const&) = nullptr;
}