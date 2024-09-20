// © 2022 Adam Badke. All rights reserved.
#include "Context_OpenGL.h"
#include "MeshPrimitive.h"
#include "VertexStream_OpenGL.h"

#include "Core/Assert.h"


namespace opengl
{
	uint32_t VertexStream::GetComponentGLDataType(re::VertexStream::DataType dataType)
	{
		switch (dataType)
		{
		case re::VertexStream::DataType::Float:		// 32-bit
		case re::VertexStream::DataType::Float2:
		case re::VertexStream::DataType::Float3:
		case re::VertexStream::DataType::Float4:
			return GL_FLOAT;

		case re::VertexStream::DataType::Int:		// 32-bit
		case re::VertexStream::DataType::Int2:
		case re::VertexStream::DataType::Int3:
		case re::VertexStream::DataType::Int4:
			return GL_INT;

		case re::VertexStream::DataType::UInt:		// 32-bit
		case re::VertexStream::DataType::UInt2:
		case re::VertexStream::DataType::UInt3:
		case re::VertexStream::DataType::UInt4:
			return GL_UNSIGNED_INT;

		case re::VertexStream::DataType::Short:		// 16-bit
		case re::VertexStream::DataType::Short2:
		case re::VertexStream::DataType::Short4:
			return GL_SHORT;

		case re::VertexStream::DataType::UShort:	// 16-bit
		case re::VertexStream::DataType::UShort2:
		case re::VertexStream::DataType::UShort4:
			return GL_UNSIGNED_SHORT;

		case re::VertexStream::DataType::Byte:		// 8-bit
		case re::VertexStream::DataType::Byte2:
		case re::VertexStream::DataType::Byte4:
			return GL_BYTE;

		case re::VertexStream::DataType::UByte:		// 8-bit
		case re::VertexStream::DataType::UByte2:
		case re::VertexStream::DataType::UByte4:
			return GL_UNSIGNED_BYTE;
		default: return std::numeric_limits<uint32_t>::max(); // Error
		}
	}


	std::unique_ptr<re::VertexStream::PlatformParams> VertexStream::CreatePlatformParams(
		re::VertexStream const&, re::VertexStream::Type)
	{
		return std::make_unique<opengl::VertexStream::PlatformParams>();
	}


	void VertexStream::Create(re::VertexStream const& vertexStream)
	{
		SEAssert(vertexStream.GetData() && vertexStream.GetNumElements() > 0, "Vertex stream has no data");

		opengl::VertexStream::PlatformParams* platformParams = 
			vertexStream.GetPlatformParams()->As<opengl::VertexStream::PlatformParams*>();

		if (platformParams->m_VBO != 0)
		{
			SEAssertF("VertexStream has already been created");
			return;
		}

		glCreateBuffers(1, &platformParams->m_VBO); // No need to bind VAO first, or bind VBO after glCreateBuffers
		
		// Buffer and label the data:
		glNamedBufferData(
			platformParams->m_VBO,				// Buffer "name"
			vertexStream.GetTotalDataByteSize(),
			vertexStream.GetData(),
			GL_STATIC_DRAW);

		glObjectLabel(
			GL_BUFFER,
			platformParams->m_VBO,
			-1,
			std::format("{} stream hash {}",
				re::VertexStream::TypeToCStr(vertexStream.GetType()), 
				vertexStream.GetDataHash()).c_str());
	}


	void VertexStream::Destroy(re::VertexStream const& vertexStream)
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


	void VertexStream::Bind(re::VertexStream const& vertexStream, uint8_t slotIdx)
	{
		SEAssert(slotIdx < re::VertexStream::k_maxVertexStreams, "OOB slot index");

		opengl::VertexStream::PlatformParams* platformParams =
			vertexStream.GetPlatformParams()->As<opengl::VertexStream::PlatformParams*>();

		if (vertexStream.GetType() == re::VertexStream::Type::Index)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, platformParams->m_VBO);
		}
		else
		{
			glBindVertexBuffer(
				slotIdx,							// Binding index
				platformParams->m_VBO,				// Buffer
				0,									// Offset
				vertexStream.GetElementByteSize()); // Stride
		}
	}
}