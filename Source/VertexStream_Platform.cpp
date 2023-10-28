// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "VertexStream_DX12.h"
#include "VertexStream_OpenGL.h"
#include "VertexStream_Platform.h"


namespace platform
{
	std::unique_ptr<re::VertexStream::PlatformParams> (*VertexStream::CreatePlatformParams)(
		re::VertexStream const&, re::VertexStream::StreamType type) = nullptr;

	void (*VertexStream::Destroy)(re::VertexStream const&) = nullptr;
}