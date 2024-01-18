// � 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "ParameterBlockAllocator_OpenGL.h"
#include "ParameterBlock_OpenGL.h"
#include "ParameterBlock.h"


namespace opengl
{
	void ParameterBlock::Create(re::ParameterBlock& paramBlock)
	{
		PlatformParams* pbPlatParams = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();
		SEAssert(!pbPlatParams->m_isCreated, "Parameter block is already created");
		pbPlatParams->m_isCreated = true;

		void const* data;
		uint32_t numBytes;
		paramBlock.GetDataAndSize(data, numBytes);

		const re::ParameterBlock::PBType pbType = paramBlock.GetType();
		switch (pbType)
		{
		case re::ParameterBlock::PBType::Mutable:
		{
			// Note: Unlike DX12, OpenGL handles buffer synchronization for us (so long as they're not persistently 
			// mapped). So we can just create a single mutable buffer and write to it as needed, instead of 
			// sub-allocating from within a larger buffer each frame

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
			SEAssert(glIsBuffer(pbPlatParams->m_bufferName) == GL_TRUE, "Failed to generate buffer object");

			glBufferData(
				bufferTarget,
				(GLsizeiptr)numBytes,
				nullptr, // NULL: Data store of the specified size is created, but remains uninitialized and thus undefined
				pbType == re::ParameterBlock::PBType::Immutable ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);

			// RenderDoc label:
			std::string const& bufferName = paramBlock.GetName() + "_Mutable";
			glObjectLabel(GL_BUFFER, pbPlatParams->m_bufferName, -1, bufferName.c_str());
		}
		break;
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
			SEAssert(glIsBuffer(pbPlatParams->m_bufferName) == GL_TRUE, "Failed to generate buffer object");

			glBufferData(
				bufferTarget,
				(GLsizeiptr)numBytes,
				nullptr, // NULL: Data store of the specified size is created, but remains uninitialized and thus undefined
				pbType == re::ParameterBlock::PBType::Immutable ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);

			// RenderDoc label:
			std::string const& bufferName = paramBlock.GetName() + "_Immutable";
			glObjectLabel(GL_BUFFER, pbPlatParams->m_bufferName, -1, bufferName.c_str());
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


	void ParameterBlock::Update(re::ParameterBlock const& paramBlock, uint8_t heapOffsetFactor)
	{
		PlatformParams* pbPlatParams = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();

		void const* data;
		uint32_t numBytes;
		paramBlock.GetDataAndSize(data, numBytes);

		//glNamedBufferSubData(
		//	pbPlatParams->m_bufferName,	// Target
		//	pbPlatParams->m_baseOffset,	// Offset
		//	(GLsizeiptr)numBytes,		// Size
		//	data);						// Data

		const GLbitfield access = GL_MAP_WRITE_BIT;

		void* cpuVisibleData = glMapNamedBufferRange(
			pbPlatParams->m_bufferName,
			pbPlatParams->m_baseOffset,
			(GLsizeiptr)numBytes,
			access);

		memcpy(cpuVisibleData, data, numBytes);

		glUnmapNamedBuffer(pbPlatParams->m_bufferName);
	}


	void ParameterBlock::Destroy(re::ParameterBlock& paramBlock)
	{
		PlatformParams* pbPlatParams = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();
		SEAssert(pbPlatParams->m_isCreated, "Attempting to destroy a ParameterBlock that has not been created");

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