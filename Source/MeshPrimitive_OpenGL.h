// © 2022 Adam Badke. All rights reserved.
#pragma once

#include <GL/glew.h>

#include "MeshPrimitive_Platform.h"
#include "MeshPrimitive.h"


namespace opengl
{
	class MeshPrimitive
	{
	public:
		struct PlatformParams final : public re::MeshPrimitive::PlatformParams
		{
			PlatformParams(re::MeshPrimitive& meshPrimitive);

			GLuint m_meshVAO = 0; // Vertex array object
			GLenum m_drawMode = GL_TRIANGLES;
		};


	public:
		static void Create(re::MeshPrimitive& meshPrimitive);
		static void Destroy(re::MeshPrimitive& meshPrimitive);

		// OpenGL-specific functionality:
		static void Bind(re::MeshPrimitive const& meshPrimitive);
	};
}