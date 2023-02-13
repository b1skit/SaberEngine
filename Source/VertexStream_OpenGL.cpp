// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "MeshPrimitive.h"
#include "VertexStream_OpenGL.h"


namespace
{
	static uint32_t GLDataType(re::VertexStream::DataType dataType)
	{
		switch (dataType)
		{
		case re::VertexStream::DataType::Float:
		{
			return GL_FLOAT;
		}
		break;
		case re::VertexStream::DataType::UInt:
		{
			return GL_UNSIGNED_INT;
		}
		break;
		case re::VertexStream::DataType::UByte:
		{
			return GL_UNSIGNED_BYTE;
		}
		break;
		default:
			SEAssertF("Invalid data type");
		}

		return GL_FLOAT;
	}
}


namespace opengl
{
	void VertexStream::Create(re::VertexStream& vertexStream, re::MeshPrimitive::Slot slot)
	{
		SEAssert("Vertex stream has no data", vertexStream.GetData() && vertexStream.GetNumElements() > 0);
		SEAssert("Invalid slot", slot != re::MeshPrimitive::Slot::Slot_Count);

		opengl::VertexStream::PlatformParams* const platformParams =
			dynamic_cast<opengl::VertexStream::PlatformParams*>(vertexStream.GetPlatformParams());

		if (platformParams->m_VBO != 0)
		{
			return; // Already created
		}

		// Generate buffer name:
		glGenBuffers(1, &platformParams->m_VBO);
		
		switch (slot)
		{
		case re::MeshPrimitive::Slot::Indexes:
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, platformParams->m_VBO);
		}
		break;
		default:
		{
			glBindBuffer(GL_ARRAY_BUFFER, platformParams->m_VBO);

			glVertexAttribPointer(						// Define array of vertex attribute data: 
				slot,									// index
				vertexStream.GetNumComponents(),		// 1/2/3/4
				GLDataType(vertexStream.GetDataType()),
				vertexStream.DoNormalize(),
				0,										// Stride
				0);
		}
		}

		glNamedBufferData(
			platformParams->m_VBO,						// Buffer "name"
			vertexStream.GetTotalDataByteSize(),
			vertexStream.GetData(),
			GL_DYNAMIC_DRAW);

		glObjectLabel(
			GL_BUFFER,
			platformParams->m_VBO,
			-1,
			re::MeshPrimitive::GetSlotDebugName(re::MeshPrimitive::Position).c_str());
	}


	void VertexStream::Destroy(re::VertexStream& vertexStream)
	{
		opengl::VertexStream::PlatformParams* const platformParams =
			dynamic_cast<opengl::VertexStream::PlatformParams*>(vertexStream.GetPlatformParams());

		if (platformParams->m_VBO == 0)
		{
			return;
		}

		glDeleteBuffers(1, &platformParams->m_VBO);
		platformParams->m_VBO = 0;
	}


	void VertexStream::Bind(re::VertexStream& vertexStream, re::MeshPrimitive::Slot slot)
	{
		Create(vertexStream, slot); // Ensure the stream is created

		opengl::VertexStream::PlatformParams* const platformParams =
			dynamic_cast<opengl::VertexStream::PlatformParams*>(vertexStream.GetPlatformParams());

		switch (slot)
		{
		case re::MeshPrimitive::Slot::Indexes:
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, platformParams->m_VBO);
		}
		break;
		default:
		{
			glBindVertexBuffer(
				slot,								// Binding index
				platformParams->m_VBO,				// Buffer
				0,									// Offset
				vertexStream.GetElementByteSize()); // Stride
		}
		}
	}
}