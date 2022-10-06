#include "Mesh_OpenGL.h"

#include "Mesh.h"
#include "DebugConfiguration.h"


namespace opengl
{
	// Platform Params:
	Mesh::PlatformParams::PlatformParams(gr::Mesh& mesh) :
		m_meshVAO(0),
		m_meshVBOs(VertexAttribute::VertexAttribute_Count, 0)
	{
		SEAssert("TODO: Support more primitive types/draw modes!", 
			mesh.GetMeshParams().m_drawMode == gr::Mesh::DrawMode::Triangles);

		switch (mesh.GetMeshParams().m_drawMode)
		{
			case gr::Mesh::DrawMode::Points:
			{
				m_drawMode = GL_POINTS;
			}
			break;
			case gr::Mesh::DrawMode::Lines:
			{
				m_drawMode = GL_LINES;
			}
			break;
			case gr::Mesh::DrawMode::LineStrip:
			{
				m_drawMode = GL_LINE_STRIP;
			}
			break;
			case gr::Mesh::DrawMode::LineLoop:
			{
				m_drawMode = GL_LINE_LOOP;
			}
			break;
			case gr::Mesh::DrawMode::Triangles:
			{
				m_drawMode = GL_TRIANGLES;
			}
			break;
			case gr::Mesh::DrawMode::TriangleStrip:
			{
				m_drawMode = GL_TRIANGLE_STRIP;
			}
			break;
			case gr::Mesh::DrawMode::TriangleFan:
			{
				m_drawMode = GL_TRIANGLE_FAN;
			}
			break;
			case gr::Mesh::DrawMode::DrawMode_Count:
			default:
				SEAssert("Unsupported draw mode", false);
		}
	}


	// Static mesh function implementations:
	/***************************************/

	void opengl::Mesh::Create(gr::Mesh& mesh)
	{
		// Create the platform params, then get a pointer to it:
		platform::Mesh::PlatformParams::CreatePlatformParams(mesh);
		opengl::Mesh::PlatformParams* const mp =
			dynamic_cast<opengl::Mesh::PlatformParams*>(mesh.GetPlatformParams().get());

		// Create a Vertex Array Object:
		glGenVertexArrays(1, &mp->m_meshVAO);
		glBindVertexArray(mp->m_meshVAO);

		// Generate names for the vertex and index buffers:
		glGenBuffers(VertexAttribute::VertexAttribute_Count, &mp->m_meshVBOs[0]);

		// Define, buffer, & label our arrays of vertex attribute data:
		//-------------------------------------------------------------
		// Position:
		SEAssert("Mesh has no vertex positions", mesh.GetPositions().size() > 0);
		glBindBuffer(GL_ARRAY_BUFFER, mp->m_meshVBOs[VertexAttribute::Position]);
		glEnableVertexArrayAttrib(mp->m_meshVAO, VertexAttribute::Position);
		glVertexAttribPointer(			// Define array of vertex attribute data: 
			VertexAttribute::Position,	// index
			3,							// number of components in the attribute
			GL_FLOAT,					// type
			GL_FALSE,					// Should data be normalized?
			0,							// Stride
			0);							// Offset from start to 1st component
		glNamedBufferData(
			mp->m_meshVBOs[opengl::Mesh::VertexAttribute::Position],
			mesh.GetPositions().size() * sizeof(float),
			&mesh.GetPositions()[0],
			GL_DYNAMIC_DRAW);
		glObjectLabel(
			GL_BUFFER,
			mp->m_meshVBOs[opengl::Mesh::VertexAttribute::Position],
			-1,
			(mesh.GetName() + " position").c_str());

		// Color:
		if (mesh.GetColors().size() > 0)
		{
			glBindBuffer(GL_ARRAY_BUFFER, mp->m_meshVBOs[VertexAttribute::Color]);
			glEnableVertexArrayAttrib(mp->m_meshVAO, VertexAttribute::Color);
			glVertexAttribPointer(
				VertexAttribute::Color,
				4,
				GL_FLOAT,
				GL_FALSE,
				0,
				0);
			glNamedBufferData(
				mp->m_meshVBOs[opengl::Mesh::VertexAttribute::Color],
				mesh.GetColors().size() * sizeof(float),
				&mesh.GetColors()[0],
				GL_DYNAMIC_DRAW);
			glObjectLabel(
				GL_BUFFER,
				mp->m_meshVBOs[opengl::Mesh::VertexAttribute::Color],
				-1,
				(mesh.GetName() + " color").c_str());
		}

		// Normals:
		if (mesh.GetNormals().size() > 0)
		{
			glBindBuffer(GL_ARRAY_BUFFER, mp->m_meshVBOs[VertexAttribute::Normal]);
			glEnableVertexArrayAttrib(mp->m_meshVAO, VertexAttribute::Normal);
			glVertexAttribPointer(
				VertexAttribute::Normal,
				3,
				GL_FLOAT,
				GL_TRUE,
				0,
				0);
			glNamedBufferData(
				mp->m_meshVBOs[opengl::Mesh::VertexAttribute::Normal],
				mesh.GetNormals().size() * sizeof(float),
				&mesh.GetNormals()[0],
				GL_DYNAMIC_DRAW);
			glObjectLabel(
				GL_BUFFER,
				mp->m_meshVBOs[opengl::Mesh::VertexAttribute::Normal],
				-1,
				(mesh.GetName() + " normal").c_str());
		}

		// Tangents:
		if (mesh.GetTangents().size() > 0)
		{

			glBindBuffer(GL_ARRAY_BUFFER, mp->m_meshVBOs[VertexAttribute::Tangent]);
			glEnableVertexArrayAttrib(mp->m_meshVAO, VertexAttribute::Tangent);

			glVertexAttribPointer(
				VertexAttribute::Tangent,
				4,
				GL_FLOAT,
				GL_TRUE,
				0,
				0);

			glNamedBufferData(
				mp->m_meshVBOs[opengl::Mesh::VertexAttribute::Tangent],
				mesh.GetTangents().size() * sizeof(float),
				&mesh.GetTangents()[0],
				GL_DYNAMIC_DRAW);
			glObjectLabel(
				GL_BUFFER,
				mp->m_meshVBOs[opengl::Mesh::VertexAttribute::Tangent],
				-1,
				(mesh.GetName() + " tangent").c_str());
		}

		// UV0:
		if (mesh.GetUV0().size() > 0)
		{
			glBindBuffer(GL_ARRAY_BUFFER, mp->m_meshVBOs[VertexAttribute::UV0]);
			glEnableVertexArrayAttrib(mp->m_meshVAO, VertexAttribute::UV0);
			glVertexAttribPointer(
				VertexAttribute::UV0,
				2, 
				GL_FLOAT,
				GL_FALSE,
				0,
				0);
			glNamedBufferData(
				mp->m_meshVBOs[opengl::Mesh::VertexAttribute::UV0],
				mesh.GetUV0().size() * sizeof(float),
				&mesh.GetUV0()[0],
				GL_DYNAMIC_DRAW);
			glObjectLabel(
				GL_BUFFER,
				mp->m_meshVBOs[opengl::Mesh::VertexAttribute::UV0],
				-1,
				(mesh.GetName() + " UV0").c_str());
		}

		// Indexes:	
		SEAssert("Mesh has no indexes", mesh.GetIndices().size() > 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mp->m_meshVBOs[opengl::Mesh::VertexAttribute::Indexes]);
		glNamedBufferData(
			mp->m_meshVBOs[opengl::Mesh::VertexAttribute::Indexes],
			mesh.GetIndices().size() * sizeof(uint32_t),
			&mesh.GetIndices()[0],
			GL_DYNAMIC_DRAW);
		glObjectLabel(
			GL_BUFFER,
			mp->m_meshVBOs[opengl::Mesh::VertexAttribute::Indexes],
			-1,
			(mesh.GetName() + " index").c_str());

		// Renderdoc name for the VAO now that everything is bound
		glObjectLabel(GL_VERTEX_ARRAY, mp->m_meshVAO, -1, (mesh.GetName() + " VAO").c_str());
	}


	void opengl::Mesh::Bind(gr::Mesh& mesh, bool doBind/*= true*/)
	{
		if (doBind)
		{
			opengl::Mesh::PlatformParams* params =
				dynamic_cast<opengl::Mesh::PlatformParams*>(mesh.GetPlatformParams().get());

			glBindVertexArray(params->m_meshVAO);
			for (size_t i = 0; i < opengl::Mesh::VertexAttribute::VertexAttribute_Count; i++)
			{
				if (i == VertexAttribute::Indexes)
				{
					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, params->m_meshVBOs[opengl::Mesh::VertexAttribute::Indexes]);
				}
				else
				{
					glBindBuffer(GL_ARRAY_BUFFER, params->m_meshVBOs[i]);
				}
			}
		}
		else
		{
			glBindVertexArray(0);
			for (size_t i = 0; i < opengl::Mesh::VertexAttribute::VertexAttribute_Count; i++)
			{
				if (i == VertexAttribute::Indexes)
				{
					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
				}
				else
				{
					glBindBuffer(GL_ARRAY_BUFFER, 0);
				}
			}

		}
	}


	void opengl::Mesh::Destroy(gr::Mesh& mesh)
	{
		opengl::Mesh::PlatformParams* mp =
			dynamic_cast<opengl::Mesh::PlatformParams*>(mesh.GetPlatformParams().get());

		glDeleteVertexArrays(1, &mp->m_meshVAO);
		glDeleteBuffers(opengl::Mesh::VertexAttribute::VertexAttribute_Count, &mp->m_meshVBOs[0]);
	}
}