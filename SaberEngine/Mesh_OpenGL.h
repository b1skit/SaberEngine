#pragma once

#include <GL/glew.h>

#include "Mesh_Platform.h"
#include "Mesh.h"


namespace gr
{
	class Mesh;
}

namespace opengl
{
	class Mesh
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

		struct PlatformParams : public virtual platform::Mesh::PlatformParams
		{
			PlatformParams(gr::Mesh& mesh);
			~PlatformParams() override = default;

			GLuint m_meshVAO; // Vertex array object

			// IDs for buffer objects that hold vertex stream data in GPU memory (equivalent to D3D vertex buffers)
			std::vector<GLuint> m_meshVBOs;

			GLenum m_drawMode = GL_TRIANGLES;
		};

		static void Create(gr::Mesh& mesh);
		static void Bind(platform::Mesh::PlatformParams const* params, bool doBind);
		static void Destroy(gr::Mesh& mesh);
	};
}