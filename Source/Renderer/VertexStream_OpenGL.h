// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "MeshPrimitive.h"
#include "VertexStream.h"

#include <GL/glew.h>


namespace opengl
{
	class VertexStream
	{
	public:
		struct PlatformParams final : public re::VertexStream::PlatformParams
		{
			// 
		};
		static std::unique_ptr<re::VertexStream::PlatformParams> CreatePlatformParams(re::VertexStream const&);
		
		static uint32_t GetComponentGLDataType(re::DataType);


	public:
		static void Create(re::VertexStream const& vertexStream);
		static void Destroy(re::VertexStream const&);

		static void Bind(re::VertexStream const&, uint8_t slotIdx);
	};
}