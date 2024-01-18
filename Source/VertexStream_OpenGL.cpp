// © 2022 Adam Badke. All rights reserved.
#include "Context_OpenGL.h"
#include "Assert.h"
#include "MeshPrimitive.h"
#include "VertexStream_OpenGL.h"


namespace opengl
{
	uint32_t VertexStream::GetGLDataType(re::VertexStream::DataType dataType)
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


	std::unique_ptr<re::VertexStream::PlatformParams> VertexStream::CreatePlatformParams(
		re::VertexStream const&, re::VertexStream::StreamType)
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

		// We're creating a single vertex stream, but we don't have knowledge of how it might be used at this point. We
		// still need a VAO bound in order to create our VBOs, so we create a dummy VAO that will be used when creating
		// all VBOs that match the configuration of the current vertex stream
		opengl::Context* oglContext = re::Context::GetAs<opengl::Context*>();
		re::VertexStream const* vertexStreamPtr = &vertexStream;
		const GLuint tempVAO = oglContext->GetCreateVAO(&vertexStreamPtr, 1, nullptr);

		glBindVertexArray(tempVAO);
		
		// Generate our buffer name, and bind it
		glGenBuffers(1, &platformParams->m_VBO);

		opengl::VertexStream::Bind(vertexStream, gr::MeshPrimitive::Slot::Position); // Use any arbitrary slot

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
			std::format("Vertex stream hash {}", vertexStream.GetDataHash()).c_str());

		// Cleanup: Unbind our dummy VAO
		glBindVertexArray(0);
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


	void VertexStream::Bind(re::VertexStream const& vertexStream, gr::MeshPrimitive::Slot slot)
	{
		opengl::VertexStream::PlatformParams* platformParams =
			vertexStream.GetPlatformParams()->As<opengl::VertexStream::PlatformParams*>();

		if (vertexStream.GetStreamType() == re::VertexStream::StreamType::Index)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, platformParams->m_VBO);
		}
		else
		{
			glBindVertexBuffer(
				slot,								// Binding index
				platformParams->m_VBO,				// Buffer
				0,									// Offset
				vertexStream.GetElementByteSize()); // Stride
		}
	}
}