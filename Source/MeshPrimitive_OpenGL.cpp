// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "MeshPrimitive.h"
#include "MeshPrimitive_OpenGL.h"
#include "VertexStream_OpenGL.h"


namespace opengl
{
	// Platform Params:
	MeshPrimitive::PlatformParams::PlatformParams(re::MeshPrimitive& meshPrimitive)
		: m_meshVAO(0)
		, m_drawMode(GL_TRIANGLES)
	{	
		SEAssert("TODO: Support more primitive types/draw modes!", 
			meshPrimitive.GetMeshParams().m_drawMode == re::MeshPrimitive::DrawMode::Triangles);

		switch (meshPrimitive.GetMeshParams().m_drawMode)
		{
			case re::MeshPrimitive::DrawMode::Points:
			{
				m_drawMode = GL_POINTS;
			}
			break;
			case re::MeshPrimitive::DrawMode::Lines:
			{
				m_drawMode = GL_LINES;
			}
			break;
			case re::MeshPrimitive::DrawMode::LineStrip:
			{
				m_drawMode = GL_LINE_STRIP;
			}
			break;
			case re::MeshPrimitive::DrawMode::LineLoop:
			{
				m_drawMode = GL_LINE_LOOP;
			}
			break;
			case re::MeshPrimitive::DrawMode::Triangles:
			{
				m_drawMode = GL_TRIANGLES;
			}
			break;
			case re::MeshPrimitive::DrawMode::TriangleStrip:
			{
				m_drawMode = GL_TRIANGLE_STRIP;
			}
			break;
			case re::MeshPrimitive::DrawMode::TriangleFan:
			{
				m_drawMode = GL_TRIANGLE_FAN;
			}
			break;
			case re::MeshPrimitive::DrawMode::DrawMode_Count:
			default:
				SEAssertF("Unsupported draw mode");
		}
	}


	void opengl::MeshPrimitive::Create(re::MeshPrimitive& meshPrimitive)
	{
		opengl::MeshPrimitive::PlatformParams* meshPlatformParams = 
			meshPrimitive.GetPlatformParams()->As<opengl::MeshPrimitive::PlatformParams*>();

		SEAssert("Mesh primitive already created", meshPlatformParams->m_meshVAO == 0);

		// Create a Vertex Array Object:
		glGenVertexArrays(1, &meshPlatformParams->m_meshVAO);
		glBindVertexArray(meshPlatformParams->m_meshVAO);

		// Create and enable our vertex buffers
		for (size_t i = 0; i < re::MeshPrimitive::Slot_Count; i++)
		{
			const re::MeshPrimitive::Slot slot = static_cast<re::MeshPrimitive::Slot>(i);
			if (meshPrimitive.GetVertexStream(slot))
			{
				VertexStream::Create(*meshPrimitive.GetVertexStream(slot), slot);

				if (slot != re::MeshPrimitive::Slot::Indexes)
				{
					glEnableVertexArrayAttrib(meshPlatformParams->m_meshVAO, slot);
				}
			}
		}

		// Renderdoc name for the VAO now that everything is bound
		glObjectLabel(GL_VERTEX_ARRAY, meshPlatformParams->m_meshVAO, -1, (meshPrimitive.GetName() + " VAO").c_str());
	}


	void opengl::MeshPrimitive::Bind(re::MeshPrimitive const& meshPrimitive)
	{
		opengl::MeshPrimitive::PlatformParams const* glMeshParams = 
			meshPrimitive.GetPlatformParams()->As<opengl::MeshPrimitive::PlatformParams const*>();

		// The VAO state describes the expected format/stride/etc of the vertex buffers at each binding index
		glBindVertexArray(glMeshParams->m_meshVAO);

		for (size_t i = 0; i < re::MeshPrimitive::Slot_Count; i++)
		{
			const re::MeshPrimitive::Slot slot = static_cast<re::MeshPrimitive::Slot>(i);
			if (meshPrimitive.GetVertexStream(slot))
			{
				opengl::VertexStream::Bind(*meshPrimitive.GetVertexStream(slot), slot);
			}
		}
	}


	void opengl::MeshPrimitive::Destroy(re::MeshPrimitive& meshPrimitive)
	{
		opengl::MeshPrimitive::PlatformParams* mp = 
			meshPrimitive.GetPlatformParams()->As<opengl::MeshPrimitive::PlatformParams*>();

		if (mp->m_meshVAO == 0)
		{
			return;
		}

		glDeleteVertexArrays(1, &mp->m_meshVAO);
		mp->m_meshVAO = 0;
	}
}