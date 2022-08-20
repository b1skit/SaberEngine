#include "Mesh_OpenGL.h"

#include "grMesh.h"

namespace opengl
{
	// Static mesh function implementations:
	/***************************************/

	void opengl::Mesh::Create(gr::Mesh& mesh)
	{
		opengl::Mesh::PlatformParams* mp =
			dynamic_cast<opengl::Mesh::PlatformParams*>(mesh.GetPlatformParams().get());

		// Create a Vertex Array Object:
		glGenVertexArrays(1, &mp->m_meshVAO);
		
		// Create a vertex buffer:
		glGenBuffers(1, &mp->m_meshVBOs[opengl::Mesh::VERTEX_BUFFER_OBJECT::BUFFER_VERTICES]);
		
		// Create an index buffer:
		glGenBuffers(1, &mp->m_meshVBOs[opengl::Mesh::VERTEX_BUFFER_OBJECT::BUFFER_INDEXES]);

		// Bind
		opengl::Mesh::Bind(mesh, true);

		// Configure:

		// Position:
		glEnableVertexAttribArray(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_POSITION);
		glVertexAttribPointer(										// Define array of vertex attribute data: 
			opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_POSITION,		// index
			3,														// number of components (3 = 3 elements in vec3)
			GL_FLOAT,												// type
			GL_FALSE,												// Should data be normalized?
			sizeof(gr::Vertex),										// stride
			(void*)offsetof(gr::Vertex, gr::Vertex::m_position));	//offset from start to 1st component

		// Color buffer:
		glEnableVertexAttribArray(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_COLOR);
		glVertexAttribPointer(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_COLOR, 
			4, 
			GL_FLOAT, 
			GL_FALSE, 
			sizeof(gr::Vertex), 
			(void*)offsetof(gr::Vertex, gr::Vertex::m_color));

		// Normals:
		glEnableVertexAttribArray(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_NORMAL);
		glVertexAttribPointer(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_NORMAL,
			3, 
			GL_FLOAT, 
			GL_TRUE, 
			sizeof(gr::Vertex), 
			(void*)offsetof(gr::Vertex, gr::Vertex::m_normal));

		// Tangents:
		glEnableVertexAttribArray(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_TANGENT);
		glVertexAttribPointer(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_TANGENT,
			3,
			GL_FLOAT,
			GL_TRUE,
			sizeof(gr::Vertex),
			(void*)offsetof(gr::Vertex, gr::Vertex::m_tangent));

		// Bitangents:
		glEnableVertexAttribArray(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_BITANGENT);
		glVertexAttribPointer(
			opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_BITANGENT,
			3, 
			GL_FLOAT,
			GL_TRUE,
			sizeof(gr::Vertex),
			(void*)offsetof(gr::Vertex, gr::Vertex::m_bitangent));

		// UV's:
		glEnableVertexAttribArray(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_UV0);
		glVertexAttribPointer(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_UV0,
			4, GL_FLOAT, 
			GL_FALSE,
			sizeof(gr::Vertex), 
			(void*)offsetof(gr::Vertex, gr::Vertex::m_uv0));
		
		glEnableVertexAttribArray(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_UV1);
		glVertexAttribPointer(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_UV1,
			4,
			GL_FLOAT, 
			GL_FALSE,
			sizeof(gr::Vertex),
			(void*)offsetof(gr::Vertex, gr::Vertex::m_uv1));

		glEnableVertexAttribArray(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_UV2);
		glVertexAttribPointer(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_UV2,
			4,
			GL_FLOAT, 
			GL_FALSE, 
			sizeof(gr::Vertex),
			(void*)offsetof(gr::Vertex, gr::Vertex::m_uv2));

		glEnableVertexAttribArray(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_UV3);
		glVertexAttribPointer(opengl::Mesh::VERTEX_ATTRIBUTE::VERTEX_UV3,
			4, 
			GL_FLOAT,
			GL_FALSE,
			sizeof(gr::Vertex), 
			(void*)offsetof(gr::Vertex, gr::Vertex::m_uv3));

		// Buffer data:
		glBufferData(GL_ARRAY_BUFFER,
			mesh.NumVerts() * sizeof(gr::Vertex),
			&mesh.Vertices()[0].m_position.x,
			GL_DYNAMIC_DRAW);

		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
			mesh.NumIndices() * sizeof(uint32_t),
			&mesh.Indices()[0],
			GL_DYNAMIC_DRAW);
	}


	void opengl::Mesh::Bind(gr::Mesh& mesh, bool doBind/*= true*/)
	{
		if (doBind)
		{
			opengl::Mesh::PlatformParams* params = 
				dynamic_cast<opengl::Mesh::PlatformParams*>(mesh.GetPlatformParams().get());

			glBindVertexArray(params->m_meshVAO);
			glBindBuffer(GL_ARRAY_BUFFER, params->m_meshVBOs[opengl::Mesh::VERTEX_BUFFER_OBJECT::BUFFER_VERTICES]);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, params->m_meshVBOs[opengl::Mesh::VERTEX_BUFFER_OBJECT::BUFFER_INDEXES]);
		}
		else
		{
			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}
	}


	void opengl::Mesh::Destroy(gr::Mesh& mesh)
	{
		opengl::Mesh::PlatformParams* mp =
			dynamic_cast<opengl::Mesh::PlatformParams*>(mesh.GetPlatformParams().get());

		glDeleteVertexArrays(1, &mp->m_meshVAO);
		glDeleteBuffers(opengl::Mesh::VERTEX_BUFFER_OBJECT::BUFFER_COUNT, &mp->m_meshVBOs[0]);
	}
}