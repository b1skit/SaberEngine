// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "VertexStream.h"

namespace platform
{
	class VertexStream
	{
	public:
		static std::unique_ptr<re::VertexStream::PlatformParams> CreatePlatformParams();


	public:
		static void (*Destroy)(re::VertexStream&);
	};
}