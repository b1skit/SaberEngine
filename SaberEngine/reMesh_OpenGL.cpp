#include "reMesh_OpenGL.h"

#include "grMesh.h"

namespace re::platform::opengl
{
	// Static mesh function implementations:
	/***************************************/

	void re::platform::opengl::Create(gr::Mesh& mesh)
	{
		re::platform::opengl::MeshParams_OpenGL* mp =
			dynamic_cast<re::platform::opengl::MeshParams_OpenGL*>(mesh.GetParams().get());

		// Create a Vertex Array Object:
		glGenVertexArrays(1, &mp->m_meshVAO);
		
		// Create a vertex buffer:
		glGenBuffers(1, &mp->m_meshVBOs[BUFFER_VERTICES]);
		
		// Create an index buffer:
		glGenBuffers(1, &mp->m_meshVBOs[BUFFER_INDEXES]);

		// Bind
		Mesh::Bind(mesh, true);

		// Configure:

		// Position:
		glEnableVertexAttribArray(VERTEX_POSITION);
		glVertexAttribPointer(										// Define array of vertex attribute data: 
			VERTEX_POSITION,										// index
			3,														// number of components (3 = 3 elements in vec3)
			GL_FLOAT,												// type
			GL_FALSE,												// Should data be normalized?
			sizeof(gr::Vertex),										// stride
			(void*)offsetof(gr::Vertex, gr::Vertex::m_position));	//offset from start to 1st component

		// Color buffer:
		glEnableVertexAttribArray(VERTEX_COLOR);
		glVertexAttribPointer(VERTEX_COLOR, 
			4, 
			GL_FLOAT, 
			GL_FALSE, 
			sizeof(gr::Vertex), 
			(void*)offsetof(gr::Vertex, gr::Vertex::m_color));

		// Normals:
		glEnableVertexAttribArray(VERTEX_NORMAL);
		glVertexAttribPointer(VERTEX_NORMAL, 
			3, 
			GL_FLOAT, 
			GL_TRUE, 
			sizeof(gr::Vertex), 
			(void*)offsetof(gr::Vertex, gr::Vertex::m_normal));

		// Tangents:
		glEnableVertexAttribArray(VERTEX_TANGENT);
		glVertexAttribPointer(VERTEX_TANGENT,
			3,
			GL_FLOAT,
			GL_TRUE,
			sizeof(gr::Vertex),
			(void*)offsetof(gr::Vertex, gr::Vertex::m_tangent));

		// Bitangents:
		glEnableVertexAttribArray(VERTEX_BITANGENT);
		glVertexAttribPointer(
			VERTEX_BITANGENT,
			3, 
			GL_FLOAT,
			GL_TRUE,
			sizeof(gr::Vertex),
			(void*)offsetof(gr::Vertex, gr::Vertex::m_bitangent));

		// UV's:
		glEnableVertexAttribArray(VERTEX_UV0);
		glVertexAttribPointer(VERTEX_UV0,
			4, GL_FLOAT, 
			GL_FALSE,
			sizeof(gr::Vertex), 
			(void*)offsetof(gr::Vertex, gr::Vertex::m_uv0));

		glEnableVertexAttribArray(VERTEX_UV1);
		glVertexAttribPointer(VERTEX_UV1, 
			4,
			GL_FLOAT, 
			GL_FALSE,
			sizeof(gr::Vertex),
			(void*)offsetof(gr::Vertex, gr::Vertex::m_uv1));

		glEnableVertexAttribArray(VERTEX_UV2);
		glVertexAttribPointer(VERTEX_UV2, 
			4,
			GL_FLOAT, 
			GL_FALSE, 
			sizeof(gr::Vertex),
			(void*)offsetof(gr::Vertex, gr::Vertex::m_uv2));

		glEnableVertexAttribArray(VERTEX_UV3);
		glVertexAttribPointer(VERTEX_UV3,
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


	void re::platform::opengl::Delete(gr::Mesh& mesh)
	{
		re::platform::opengl::MeshParams_OpenGL* mp =
			dynamic_cast<re::platform::opengl::MeshParams_OpenGL*>(mesh.GetParams().get());

		glDeleteVertexArrays(1, &mp->m_meshVAO);
		glDeleteBuffers(BUFFER_COUNT, &mp->m_meshVBOs[0]);
	}


	void re::platform::opengl::Bind(gr::Mesh& mesh, bool doBind/*= true*/)
	{
		if (doBind)
		{
			re::platform::opengl::MeshParams_OpenGL* params = 
				dynamic_cast<re::platform::opengl::MeshParams_OpenGL*>(mesh.GetParams().get());

			glBindVertexArray(params->m_meshVAO);
			glBindBuffer(GL_ARRAY_BUFFER, params->m_meshVBOs[BUFFER_VERTICES]);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, params->m_meshVBOs[BUFFER_INDEXES]);
		}
		else
		{
			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}
	}
}