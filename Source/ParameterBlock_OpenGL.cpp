// © 2022 Adam Badke. All rights reserved.
#include "ParameterBlockAllocator_OpenGL.h"
#include "ParameterBlock_OpenGL.h"
#include "ParameterBlock.h"
#include "Assert.h"


namespace opengl
{
	void ParameterBlock::Create(re::ParameterBlock& paramBlock)
	{
		PlatformParams* pbPlatParams = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();
		SEAssert("Parameter block is already created", !pbPlatParams->m_isCreated);
		pbPlatParams->m_isCreated = true;

		void const* data;
		uint32_t numBytes;
		paramBlock.GetDataAndSize(data, numBytes);

		const re::ParameterBlock::PBType pbType = paramBlock.GetType();
		switch (pbType)
		{
		case re::ParameterBlock::PBType::Mutable:
		case re::ParameterBlock::PBType::Immutable:
		{
			// Generate the buffer name:
			glGenBuffers(1, &pbPlatParams->m_bufferName);

			pbPlatParams->m_baseOffset = 0; // Permanent PBs have their own dedicated buffers

			GLenum bufferTarget = 0;
			switch (pbPlatParams->m_dataType)
			{
			case re::ParameterBlock::PBDataType::SingleElement:
			{
				bufferTarget = GL_UNIFORM_BUFFER;
			}
			break;
			case re::ParameterBlock::PBDataType::Array:
			{
				bufferTarget = GL_SHADER_STORAGE_BUFFER;
			}
			break;
			default: SEAssertF("Invalid PBDataType");
			}

			// Binding associates the buffer object with the buffer object name
			glBindBuffer(bufferTarget, pbPlatParams->m_bufferName);
			SEAssert("Failed to generate buffer object", glIsBuffer(pbPlatParams->m_bufferName) == GL_TRUE);

			glBufferData(
				bufferTarget,
				(GLsizeiptr)numBytes,
				nullptr, // NULL: Data store of the specified size is created, but remains uninitialized and thus undefined
				pbType == re::ParameterBlock::PBType::Immutable ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);

			// RenderDoc label:
			glObjectLabel(GL_BUFFER, pbPlatParams->m_bufferName, -1, paramBlock.GetName().c_str());
		}
		break;
		case re::ParameterBlock::PBType::SingleFrame:
		{
			opengl::ParameterBlockAllocator::GetSubAllocation(
				pbPlatParams->m_dataType,
				numBytes,
				pbPlatParams->m_bufferName,
				pbPlatParams->m_baseOffset);
		}
		break;
		default: SEAssertF("Invalid PBType");
		}
	}


	void ParameterBlock::Update(re::ParameterBlock const& paramBlock)
	{
		PlatformParams* pbPlatParams = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();

		void const* data;
		uint32_t numBytes;
		paramBlock.GetDataAndSize(data, numBytes);

		// TODO: We could switch this to map to mirror the DX12 implementation, but it's a little less straightforward
		// https://registry.khronos.org/OpenGL-Refpages/gl4/html/glMapBufferRange.xhtml
		// https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming
		
		glNamedBufferSubData(
			pbPlatParams->m_bufferName,	// Target
			pbPlatParams->m_baseOffset,	// Offset
			(GLsizeiptr)numBytes,		// Size
			data);						// Data
	}


	void ParameterBlock::Destroy(re::ParameterBlock& paramBlock)
	{
		PlatformParams* pbPlatParams = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();
		SEAssert("Attempting to destroy a ParameterBlock that has not been created", pbPlatParams->m_isCreated);

		const re::ParameterBlock::PBType pbType = paramBlock.GetType();
		switch (pbType)
		{
		case re::ParameterBlock::PBType::Mutable:
		case re::ParameterBlock::PBType::Immutable:
		{
			glDeleteBuffers(1, &pbPlatParams->m_bufferName);
		}
		break;
		case re::ParameterBlock::PBType::SingleFrame:
		{
			// Do nothing: Parameter block allocator is responsible for destroying the shared buffers
		}
		break;
		default: SEAssertF("Invalid PBType");
		}

		pbPlatParams->m_bufferName = 0;
		pbPlatParams->m_baseOffset = 0;
		pbPlatParams->m_isCreated = false;
	}


	void ParameterBlock::Bind(re::ParameterBlock const& paramBlock, GLuint bindIndex)
	{
		PlatformParams* pbPlatParams = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();
		 
		GLenum bufferTarget = 0;
		switch (pbPlatParams->m_dataType)
		{
		case re::ParameterBlock::PBDataType::SingleElement:
		{
			bufferTarget = GL_UNIFORM_BUFFER;
		}
		break;
		case re::ParameterBlock::PBDataType::Array:
		{
			bufferTarget = GL_SHADER_STORAGE_BUFFER;
		}
		break;
		default: SEAssertF("Invalid PBDataType");
		}

		void const* data;
		uint32_t numBytes;
		paramBlock.GetDataAndSize(data, numBytes);
		glBindBufferRange(bufferTarget, bindIndex, pbPlatParams->m_bufferName, pbPlatParams->m_baseOffset, numBytes);
	}
}