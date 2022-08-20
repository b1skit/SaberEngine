#pragma once

#include <GL/glew.h>

#include "Mesh_Platform.h"
#include "grMesh.h"

namespace gr
{
	class Mesh;
}

namespace opengl
{
	class Mesh
	{
	public:
		enum VERTEX_BUFFER_OBJECT
		{
			BUFFER_VERTICES,
			BUFFER_INDEXES,

			BUFFER_COUNT, // Reserved: Number of buffers to allocate
		};


		enum VERTEX_ATTRIBUTE
		{
			VERTEX_POSITION = 0,
			VERTEX_COLOR = 1,

			VERTEX_NORMAL = 2,
			VERTEX_TANGENT = 3,
			VERTEX_BITANGENT = 4,

			VERTEX_UV0 = 5, // TODO: Support multiple UV channels?
			VERTEX_UV1 = 6,
			VERTEX_UV2 = 7,
			VERTEX_UV3 = 8,

			VERTEX_ATTRIBUTES_COUNT	// RESERVED: The total number of vertex attributes
		};



		struct PlatformParams : public virtual platform::Mesh::PlatformParams
		{
			~PlatformParams() override = default;

			// Vertex array object:
			GLuint m_meshVAO = 0;

			// IDs for buffer objects that hold vertices in GPU memory (equivalent to D3D vertex buffers)
			std::vector<GLuint> m_meshVBOs = std::vector<GLuint>(opengl::Mesh::BUFFER_COUNT, 0);
		};


		// Creates VAO and vertex/index VBOs, and buffers the data. Mesh remains bound at completion
		static void Create(gr::Mesh& mesh);

		// Binds/unbinds the VAO, and vertex/index VBOs
		static void Bind(gr::Mesh& mesh, bool doBind = true);

		// Deletes VAO and vertex/index VBOs and associated GPU resources
		static void Destroy(gr::Mesh& mesh);
	};
}