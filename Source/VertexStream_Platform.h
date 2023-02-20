// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "VertexStream.h"
#include "MeshPrimitive.h"


namespace platform
{
	class VertexStream
	{
	public:
		static std::unique_ptr<re::VertexStream::PlatformParams> (*CreatePlatformParams)(re::VertexStream::StreamType type);


	public:
		static void (*Destroy)(re::VertexStream&);
	};
}