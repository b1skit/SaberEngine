// © 2022 Adam Badke. All rights reserved.
#pragma once

#include <GL/glew.h>

#include "MeshPrimitive.h"
#include "VertexStream.h"


namespace opengl
{
	class VertexStream
	{
	public:
		struct PlatformParams final : public re::VertexStream::PlatformParams
		{
			GLuint m_VBO = 0;
		};
		static std::unique_ptr<re::VertexStream::PlatformParams> CreatePlatformParams(re::VertexStream const&, re::VertexStream::StreamType);
		
		static uint32_t GetGLDataType(re::VertexStream::DataType dataType);


	public:
		static void Create(re::VertexStream const& vertexStream);
		static void Destroy(re::VertexStream const&);

		static void Bind(re::VertexStream const&, gr::MeshPrimitive::Slot);
	};
}