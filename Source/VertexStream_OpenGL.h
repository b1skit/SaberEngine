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
		struct PlatformParams final : public virtual re::VertexStream::PlatformParams
		{
			GLuint m_VBO = 0;
		};

		
	public:
		static void Create(re::VertexStream& vertexStream, re::MeshPrimitive::Slot);
		static void Destroy(re::VertexStream&);

		static void Bind(re::VertexStream&, re::MeshPrimitive::Slot);
	};
}