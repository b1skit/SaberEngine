#include "grMesh.h"
#include "BuildConfiguration.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
using glm::pi;


namespace gr
{
	// Returns a Bounds, transformed from local space using transform
	Bounds Bounds::GetTransformedBounds(mat4 const& m_transform)
	{
		// Temp: Ensure the bounds are 3D here, before we do any calculations
		Make3Dimensional();

		Bounds result;

		vec4 m_points[8];																// "front" == fwd == Z -
		m_points[0] = vec4(m_xMin, m_yMax, m_zMin, 1.0f);		// Left		top		front 
		m_points[1] = vec4(m_xMax, m_yMax, m_zMin, 1.0f);		// Right	top		front
		m_points[2] = vec4(m_xMin, m_yMin, m_zMin, 1.0f);		// Left		bot		front
		m_points[3] = vec4(m_xMax, m_yMin, m_zMin, 1.0f);		// Right	bot		front

		m_points[4] = vec4(m_xMin, m_yMax, m_zMax, 1.0f);		// Left		top		back
		m_points[5] = vec4(m_xMax, m_yMax, m_zMax, 1.0f);		// Right	top		back
		m_points[6] = vec4(m_xMin, m_yMin, m_zMax, 1.0f);		// Left		bot		back
		m_points[7] = vec4(m_xMax, m_yMin, m_zMax, 1.0f);		// Right	bot		back

		for (int i = 0; i < 8; i++)
		{
			m_points[i] = m_transform * m_points[i];

			if (m_points[i].x < result.m_xMin)
			{
				result.m_xMin = m_points[i].x;
			}
			if (m_points[i].x > result.m_xMax)
			{
				result.m_xMax = m_points[i].x;
			}

			if (m_points[i].y < result.m_yMin)
			{
				result.m_yMin = m_points[i].y;
			}
			if (m_points[i].y > result.m_yMax)
			{
				result.m_yMax = m_points[i].y;
			}

			if (m_points[i].z < result.m_zMin)
			{
				result.m_zMin = m_points[i].z;
			}
			if (m_points[i].z > result.m_zMax)
			{
				result.m_zMax = m_points[i].z;
			}
		}

		return result;
	}


	void Bounds::Make3Dimensional()
	{
		float depthBias = 0.01f;
		if (glm::abs(m_xMax - m_xMin) < depthBias)
		{
			m_xMax += depthBias;
			m_xMin -= depthBias;
		}

		if (glm::abs(m_yMax - m_yMin) < depthBias)
		{
			m_yMax += depthBias;
			m_yMin -= depthBias;
		}

		if (glm::abs(m_zMax - m_zMin) < depthBias)
		{
			m_zMax += depthBias;
			m_zMin -= depthBias;
		}
	}


	Mesh::Mesh(string name, std::vector<Vertex> vertices, std::vector<GLuint> indices, SaberEngine::Material* newMeshMaterial)
	{
		meshName		= name;

		m_vertices = vertices;
		m_indices = indices;

		m_meshMaterial	= newMeshMaterial;

		// Once we've stored our properties locally, we can compute the localBounds:
		ComputeBounds();


		// Create and bind our Vertex Array Object:
		glGenVertexArrays(1, &m_meshVAO);
		glBindVertexArray(m_meshVAO);

		// Create and bind a vertex buffer:
		glGenBuffers(1, &m_meshVBOs[BUFFER_VERTICES]);
		glBindBuffer(GL_ARRAY_BUFFER, m_meshVBOs[BUFFER_VERTICES]);

		// Create and bind an index buffer:
		glGenBuffers(1, &m_meshVBOs[BUFFER_INDEXES]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_meshVBOs[BUFFER_INDEXES]);


		// Position:
		glEnableVertexAttribArray(VERTEX_POSITION);
		glVertexAttribPointer(VERTEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_position)); // Define array of vertex attribute data: index, number of components (3 = 3 elements in vec3), type, should data be normalized?, stride, offset from start to 1st component

		// Color buffer:
		glEnableVertexAttribArray(VERTEX_COLOR);
		glVertexAttribPointer(VERTEX_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_color));

		// Normals:
		glEnableVertexAttribArray(VERTEX_NORMAL);
		glVertexAttribPointer(VERTEX_NORMAL, 3, GL_FLOAT, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, m_normal));

		// Tangents:
		glEnableVertexAttribArray(VERTEX_TANGENT);
		glVertexAttribPointer(VERTEX_TANGENT, 3, GL_FLOAT, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, m_tangent));

		// Bitangents:
		glEnableVertexAttribArray(VERTEX_BITANGENT);
		glVertexAttribPointer(VERTEX_BITANGENT, 3, GL_FLOAT, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, m_bitangent));

		// UV's:
		glEnableVertexAttribArray(VERTEX_UV0);
		glVertexAttribPointer(VERTEX_UV0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_uv0));

		glEnableVertexAttribArray(VERTEX_UV1);
		glVertexAttribPointer(VERTEX_UV1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_uv1));
		
		glEnableVertexAttribArray(VERTEX_UV2);
		glVertexAttribPointer(VERTEX_UV2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_uv2));
		
		glEnableVertexAttribArray(VERTEX_UV3);
		glVertexAttribPointer(VERTEX_UV3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_uv3));


		// Buffer data:
		glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), &vertices[0].m_position.x, GL_DYNAMIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(GLuint), &indices[0], GL_DYNAMIC_DRAW);


		// Cleanup:
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}


	void Mesh::Bind(bool doBind)
	{
		if (doBind)
		{
			glBindVertexArray(VAO());
			glBindBuffer(GL_ARRAY_BUFFER, VBO(BUFFER_VERTICES));
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBO(BUFFER_INDEXES));
		}
		else
		{
			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}
	}


	void Mesh::Destroy()
	{
		#if defined(DEBUG_LOG_OUTPUT)
			meshName = meshName + "_DELETED"; // Safety...
		#endif

		if (m_vertices.size() > 0)
		{
			m_vertices.clear();
		}
		if (m_indices.size() > 0)
		{
			m_indices.clear();
		}

		glDeleteVertexArrays(1, &m_meshVAO);
		glDeleteBuffers(BUFFER_COUNT, m_meshVBOs);

		m_meshMaterial = nullptr;		// Note: Material MUST be cleaned up elsewhere!
	}

	void Mesh::ComputeBounds()
	{
		for (unsigned int i = 0; i < m_vertices.size(); i++)
		{
			if (m_vertices[i].m_position.x < m_localBounds.xMin())
			{
				m_localBounds.xMin() = m_vertices[i].m_position.x;
			}
			if (m_vertices[i].m_position.x > m_localBounds.xMax())
			{
				m_localBounds.xMax() = m_vertices[i].m_position.x;
			}

			if (m_vertices[i].m_position.y < m_localBounds.yMin())
			{
				m_localBounds.yMin() = m_vertices[i].m_position.y;
			}
			if (m_vertices[i].m_position.y > m_localBounds.yMax())
			{
				m_localBounds.yMax() = m_vertices[i].m_position.y;
			}

			if (m_vertices[i].m_position.z < m_localBounds.zMin())
			{
				m_localBounds.zMin() = m_vertices[i].m_position.z;
			}
			if (m_vertices[i].m_position.z > m_localBounds.zMax())
			{
				m_localBounds.zMax() = m_vertices[i].m_position.z;
			}
		}
	}
}


