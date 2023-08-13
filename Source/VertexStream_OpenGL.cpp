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
	std::unique_ptr<re::VertexStream::PlatformParams> VertexStream::CreatePlatformParams(
		re::VertexStream const&, re::VertexStream::StreamType)
	{
		return std::make_unique<opengl::VertexStream::PlatformParams>();
	}


	void VertexStream::Create(re::VertexStream& vertexStream, re::MeshPrimitive::Slot slot)
	{
		SEAssert("Vertex stream has no data", vertexStream.GetData() && vertexStream.GetNumElements() > 0);
		SEAssert("Invalid slot", slot != re::MeshPrimitive::Slot::Slot_Count);

		opengl::VertexStream::PlatformParams* platformParams = 
			vertexStream.GetPlatformParams()->As<opengl::VertexStream::PlatformParams*>();

		if (platformParams->m_VBO != 0)
		{
			SEAssertF("VertexStream has already been created");
			return;
		}

		// Generate our buffer name, and bind it
		glGenBuffers(1, &platformParams->m_VBO);
		
		opengl::VertexStream::Bind(vertexStream, slot);

		// Define our vertex layout:
		if (slot != re::MeshPrimitive::Slot::Indexes)
		{
			glVertexAttribFormat(
				slot,									// Attribute index
				vertexStream.GetNumComponents(),		// size: 1/2/3/4 
				GLDataType(vertexStream.GetDataType()),	// Data type
				vertexStream.DoNormalize(),				// Should the data be normalized?
				0);										// relativeOffset: Distance between buffer elements

			glVertexAttribBinding(
				slot,		// Attribue index: The actual vertex attribute index = [0, GL_MAX_VERTEX_ATTRIBS​ - 1]
				slot);		// Binding index: NOT a vertex attribute [0, GL_MAX_VERTEX_ATTRIB_BINDINGS - 1]
		}

		// Buffer and label the data:
		glNamedBufferData(
			platformParams->m_VBO,						// Buffer "name"
			vertexStream.GetTotalDataByteSize(),
			vertexStream.GetData(),
			GL_STATIC_DRAW);

		glObjectLabel(
			GL_BUFFER,
			platformParams->m_VBO,
			-1,
			re::MeshPrimitive::GetSlotDebugName(slot).c_str());
	}


	void VertexStream::Destroy(re::VertexStream& vertexStream)
	{
		opengl::VertexStream::PlatformParams* platformParams =
			vertexStream.GetPlatformParams()->As<opengl::VertexStream::PlatformParams*>();

		if (platformParams->m_VBO == 0)
		{
			return;
		}

		glDeleteBuffers(1, &platformParams->m_VBO);
		platformParams->m_VBO = 0;
	}


	void VertexStream::Bind(re::VertexStream& vertexStream, re::MeshPrimitive::Slot slot)
	{
		opengl::VertexStream::PlatformParams* platformParams =
			vertexStream.GetPlatformParams()->As<opengl::VertexStream::PlatformParams*>();

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