// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "VertexStream.h"
#include "MeshPrimitive.h"


namespace dx12
{
	class VertexStream
	{
	public:
		struct PlatformParams final : public virtual re::VertexStream::PlatformParams
		{
		};


	public:
		static void Create(re::VertexStream& vertexStream, re::MeshPrimitive::Slot);
		static void Destroy(re::VertexStream&);
	};
}