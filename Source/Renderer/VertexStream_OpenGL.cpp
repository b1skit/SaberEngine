// © 2022 Adam Badke. All rights reserved.
#include "Buffer_OpenGL.h"
#include "Context_OpenGL.h"
#include "MeshPrimitive.h"
#include "VertexStream_OpenGL.h"

#include "Core/Assert.h"


namespace opengl
{
	uint32_t VertexStream::GetComponentGLDataType(re::DataType dataType)
	{
		switch (dataType)
		{
		case re::DataType::Float:		// 32-bit
		case re::DataType::Float2:
		case re::DataType::Float3:
		case re::DataType::Float4:
			return GL_FLOAT;

		case re::DataType::Int:			// 32-bit
		case re::DataType::Int2:
		case re::DataType::Int3:
		case re::DataType::Int4:
			return GL_INT;

		case re::DataType::UInt:		// 32-bit
		case re::DataType::UInt2:
		case re::DataType::UInt3:
		case re::DataType::UInt4:
			return GL_UNSIGNED_INT;

		case re::DataType::Short:		// 16-bit
		case re::DataType::Short2:
		case re::DataType::Short4:
			return GL_SHORT;

		case re::DataType::UShort:		// 16-bit
		case re::DataType::UShort2:
		case re::DataType::UShort4:
			return GL_UNSIGNED_SHORT;

		case re::DataType::Byte:		// 8-bit
		case re::DataType::Byte2:
		case re::DataType::Byte4:
			return GL_BYTE;

		case re::DataType::UByte:		// 8-bit
		case re::DataType::UByte2:
		case re::DataType::UByte4:
			return GL_UNSIGNED_BYTE;
		default: return std::numeric_limits<uint32_t>::max(); // Error
		}
	}


	std::unique_ptr<re::VertexStream::PlatformParams> VertexStream::CreatePlatformParams(re::VertexStream const&)
	{
		return std::make_unique<opengl::VertexStream::PlatformParams>();
	}


	void VertexStream::Create(re::VertexStream const& vertexStream)
	{
		// Do nothing; The heavy lifting is handled by the re::Buffer
	}


	void VertexStream::Destroy(re::VertexStream const& vertexStream)
	{
		// 
	}


	void VertexStream::Bind(re::VertexStream const& vertexStream, uint8_t slotIdx)
	{
		SEAssert(slotIdx < re::VertexStream::k_maxVertexStreams, "OOB slot index");

		re::Buffer const* streamBuffer = vertexStream.GetBuffer();
		SEAssert(streamBuffer, "Vertex stream buffer cannot be null");

		re::Buffer::BufferParams const& streamBufferParams = streamBuffer->GetBufferParams();
		SEAssert(re::Buffer::HasUsageBit(re::Buffer::VertexStream, streamBufferParams),
			"Buffer does not have the vertex stream usage bit set");

		opengl::Buffer::PlatformParams* streamBufferPlatParams = 
			streamBuffer->GetPlatformParams()->As<opengl::Buffer::PlatformParams*>();
		SEAssert(streamBufferPlatParams->m_baseOffset == 0, "Base offset != 0. This is unexpected");

		switch (streamBufferParams.m_vertexStreamParams.m_type)
		{
		case re::VertexStream::Index:
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, streamBufferPlatParams->m_bufferName);
		}
		break;
		default:
		{
			glBindVertexBuffer(
				slotIdx,																// Slot index
				streamBufferPlatParams->m_bufferName,									// Buffer
				0,																		// Offset
				DataTypeToStride(streamBufferParams.m_vertexStreamParams.m_dataType));	// Stride
		}
		}
	}
}