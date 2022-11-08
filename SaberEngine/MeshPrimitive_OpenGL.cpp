#include "MeshPrimitive_OpenGL.h"

#include "MeshPrimitive.h"
#include "DebugConfiguration.h"


namespace opengl
{
	// Platform Params:
	MeshPrimitive::PlatformParams::PlatformParams(gr::MeshPrimitive& meshPrimitive) :
		m_meshVAO(0),
		m_meshVBOs(VertexAttribute::VertexAttribute_Count, 0)
	{
		SEAssert("TODO: Support more primitive types/draw modes!", 
			meshPrimitive.GetMeshParams().m_drawMode == gr::MeshPrimitive::DrawMode::Triangles);

		switch (meshPrimitive.GetMeshParams().m_drawMode)
		{
			case gr::MeshPrimitive::DrawMode::Points:
			{
				m_drawMode = GL_POINTS;
			}
			break;
			case gr::MeshPrimitive::DrawMode::Lines:
			{
				m_drawMode = GL_LINES;
			}
			break;
			case gr::MeshPrimitive::DrawMode::LineStrip:
			{
				m_drawMode = GL_LINE_STRIP;
			}
			break;
			case gr::MeshPrimitive::DrawMode::LineLoop:
			{
				m_drawMode = GL_LINE_LOOP;
			}
			break;
			case gr::MeshPrimitive::DrawMode::Triangles:
			{
				m_drawMode = GL_TRIANGLES;
			}
			break;
			case gr::MeshPrimitive::DrawMode::TriangleStrip:
			{
				m_drawMode = GL_TRIANGLE_STRIP;
			}
			break;
			case gr::MeshPrimitive::DrawMode::TriangleFan:
			{
				m_drawMode = GL_TRIANGLE_FAN;
			}
			break;
			case gr::MeshPrimitive::DrawMode::DrawMode_Count:
			default:
				SEAssertF("Unsupported draw mode");
		}
	}


	// Static meshPrimitive function implementations:
	/***************************************/

	void opengl::MeshPrimitive::Create(gr::MeshPrimitive& meshPrimitive)
	{
		// Create the platform params, then get a pointer to it:
		platform::MeshPrimitive::PlatformParams::CreatePlatformParams(meshPrimitive);
		opengl::MeshPrimitive::PlatformParams* const mp =
			dynamic_cast<opengl::MeshPrimitive::PlatformParams*>(meshPrimitive.GetPlatformParams().get());

		// Create a Vertex Array Object:
		glGenVertexArrays(1, &mp->m_meshVAO);
		glBindVertexArray(mp->m_meshVAO);

		// Generate names for the vertex and index buffers:
		glGenBuffers(VertexAttribute::VertexAttribute_Count, &mp->m_meshVBOs[0]);

		// Define, buffer, & label our arrays of vertex attribute data:
		//-------------------------------------------------------------
		// Position:
		SEAssert("MeshPrimitive has no vertex positions", meshPrimitive.GetPositions().size() > 0);
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
			mp->m_meshVBOs[opengl::MeshPrimitive::VertexAttribute::Position],
			meshPrimitive.GetPositions().size() * sizeof(float),
			&meshPrimitive.GetPositions()[0],
			GL_DYNAMIC_DRAW);
		glObjectLabel(
			GL_BUFFER,
			mp->m_meshVBOs[opengl::MeshPrimitive::VertexAttribute::Position],
			-1,
			(meshPrimitive.GetName() + " position").c_str());

		// Color:
		if (meshPrimitive.GetColors().size() > 0)
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
				mp->m_meshVBOs[opengl::MeshPrimitive::VertexAttribute::Color],
				meshPrimitive.GetColors().size() * sizeof(float),
				&meshPrimitive.GetColors()[0],
				GL_DYNAMIC_DRAW);
			glObjectLabel(
				GL_BUFFER,
				mp->m_meshVBOs[opengl::MeshPrimitive::VertexAttribute::Color],
				-1,
				(meshPrimitive.GetName() + " color").c_str());
		}

		// Normals:
		if (meshPrimitive.GetNormals().size() > 0)
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
				mp->m_meshVBOs[opengl::MeshPrimitive::VertexAttribute::Normal],
				meshPrimitive.GetNormals().size() * sizeof(float),
				&meshPrimitive.GetNormals()[0],
				GL_DYNAMIC_DRAW);
			glObjectLabel(
				GL_BUFFER,
				mp->m_meshVBOs[opengl::MeshPrimitive::VertexAttribute::Normal],
				-1,
				(meshPrimitive.GetName() + " normal").c_str());
		}

		// Tangents:
		if (meshPrimitive.GetTangents().size() > 0)
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
				mp->m_meshVBOs[opengl::MeshPrimitive::VertexAttribute::Tangent],
				meshPrimitive.GetTangents().size() * sizeof(float),
				&meshPrimitive.GetTangents()[0],
				GL_DYNAMIC_DRAW);
			glObjectLabel(
				GL_BUFFER,
				mp->m_meshVBOs[opengl::MeshPrimitive::VertexAttribute::Tangent],
				-1,
				(meshPrimitive.GetName() + " tangent").c_str());
		}

		// UV0:
		if (meshPrimitive.GetUV0().size() > 0)
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
				mp->m_meshVBOs[opengl::MeshPrimitive::VertexAttribute::UV0],
				meshPrimitive.GetUV0().size() * sizeof(float),
				&meshPrimitive.GetUV0()[0],
				GL_DYNAMIC_DRAW);
			glObjectLabel(
				GL_BUFFER,
				mp->m_meshVBOs[opengl::MeshPrimitive::VertexAttribute::UV0],
				-1,
				(meshPrimitive.GetName() + " UV0").c_str());
		}

		// Indexes:	
		SEAssert("MeshPrimitive has no indexes", meshPrimitive.GetIndices().size() > 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mp->m_meshVBOs[opengl::MeshPrimitive::VertexAttribute::Indexes]);
		glNamedBufferData(
			mp->m_meshVBOs[opengl::MeshPrimitive::VertexAttribute::Indexes],
			meshPrimitive.GetIndices().size() * sizeof(uint32_t),
			&meshPrimitive.GetIndices()[0],
			GL_DYNAMIC_DRAW);
		glObjectLabel(
			GL_BUFFER,
			mp->m_meshVBOs[opengl::MeshPrimitive::VertexAttribute::Indexes],
			-1,
			(meshPrimitive.GetName() + " index").c_str());

		// Renderdoc name for the VAO now that everything is bound
		glObjectLabel(GL_VERTEX_ARRAY, mp->m_meshVAO, -1, (meshPrimitive.GetName() + " VAO").c_str());
	}


	void opengl::MeshPrimitive::Bind(platform::MeshPrimitive::PlatformParams const* params, bool doBind)
	{
		if (doBind)
		{
			opengl::MeshPrimitive::PlatformParams const* const glMeshParams =
				dynamic_cast<opengl::MeshPrimitive::PlatformParams const* const>(params);

			glBindVertexArray(glMeshParams->m_meshVAO);
			for (size_t i = 0; i < opengl::MeshPrimitive::VertexAttribute::VertexAttribute_Count; i++)
			{
				if (i == VertexAttribute::Indexes)
				{
					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glMeshParams->m_meshVBOs[opengl::MeshPrimitive::VertexAttribute::Indexes]);
				}
				else
				{
					glBindBuffer(GL_ARRAY_BUFFER, glMeshParams->m_meshVBOs[i]);
				}
			}
		}
		else
		{
			glBindVertexArray(0);
			for (size_t i = 0; i < opengl::MeshPrimitive::VertexAttribute::VertexAttribute_Count; i++)
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


	void opengl::MeshPrimitive::Destroy(gr::MeshPrimitive& meshPrimitive)
	{
		opengl::MeshPrimitive::PlatformParams* mp =
			dynamic_cast<opengl::MeshPrimitive::PlatformParams*>(meshPrimitive.GetPlatformParams().get());

		glDeleteVertexArrays(1, &mp->m_meshVAO);
		glDeleteBuffers(opengl::MeshPrimitive::VertexAttribute::VertexAttribute_Count, &mp->m_meshVBOs[0]);
	}
}