#pragma once

#include <GL/glew.h>

#include "MeshPrimitive_Platform.h"
#include "MeshPrimitive.h"


namespace gr
{
	class MeshPrimitive;
}

namespace opengl
{
	class MeshPrimitive
	{
	public:
		enum VertexAttribute
		{
			Position = 0,
			Color = 1,
			Normal = 2,
			Tangent = 3,
			UV0 = 4,

			Indexes, // Not assigned a layout binding location

			VertexAttribute_Count
		};
		// Note: The order/indexing of this enum MUST match the vertex layout locations in SaberCommon.glsl

		struct PlatformParams : public virtual platform::MeshPrimitive::PlatformParams
		{
			PlatformParams(re::MeshPrimitive& meshPrimitive);
			~PlatformParams() override = default;

			GLuint m_meshVAO; // Vertex array object

			// IDs for buffer objects that hold vertex stream data in GPU memory (equivalent to D3D vertex buffers)
			std::vector<GLuint> m_meshVBOs;

			GLenum m_drawMode = GL_TRIANGLES;
		};

		static void Create(re::MeshPrimitive& meshPrimitive);
		static void Bind(platform::MeshPrimitive::PlatformParams const* params, bool doBind);
		static void Destroy(re::MeshPrimitive& meshPrimitive);
	};
}