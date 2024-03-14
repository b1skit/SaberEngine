// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "BufferAllocator_OpenGL.h"
#include "Buffer_OpenGL.h"
#include "Buffer.h"


namespace opengl
{
	void Buffer::Create(re::Buffer& buffer)
	{
		PlatformParams* bufferPlatParams = buffer.GetPlatformParams()->As<opengl::Buffer::PlatformParams*>();
		SEAssert(!bufferPlatParams->m_isCreated, "Buffer is already created");
		bufferPlatParams->m_isCreated = true;

		void const* data;
		uint32_t numBytes;
		buffer.GetDataAndSize(data, numBytes);

		const re::Buffer::Type bufferType = buffer.GetType();
		switch (bufferType)
		{
		case re::Buffer::Type::Mutable:
		{
			// Note: Unlike DX12, OpenGL handles buffer synchronization for us (so long as they're not persistently 
			// mapped). So we can just create a single mutable buffer and write to it as needed, instead of 
			// sub-allocating from within a larger buffer each frame

			// Generate the buffer name:
			glGenBuffers(1, &bufferPlatParams->m_bufferName);

			bufferPlatParams->m_baseOffset = 0; // Permanent buffers have their own dedicated buffers

			GLenum bufferTarget = 0;
			switch (bufferPlatParams->m_dataType)
			{
			case re::Buffer::DataType::SingleElement:
			{
				bufferTarget = GL_UNIFORM_BUFFER;
			}
			break;
			case re::Buffer::DataType::Array:
			{
				bufferTarget = GL_SHADER_STORAGE_BUFFER;
			}
			break;
			default: SEAssertF("Invalid DataType");
			}

			// Binding associates the buffer object with the buffer object name
			glBindBuffer(bufferTarget, bufferPlatParams->m_bufferName);
			SEAssert(glIsBuffer(bufferPlatParams->m_bufferName) == GL_TRUE, "Failed to generate buffer object");

			glBufferData(
				bufferTarget,
				(GLsizeiptr)numBytes,
				nullptr, // NULL: Data store of the specified size is created, but remains uninitialized and thus undefined
				bufferType == re::Buffer::Type::Immutable ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);

			// RenderDoc label:
			std::string const& bufferName = buffer.GetName() + "_Mutable";
			glObjectLabel(GL_BUFFER, bufferPlatParams->m_bufferName, -1, bufferName.c_str());
		}
		break;
		case re::Buffer::Type::Immutable:
		{
			// Generate the buffer name:
			glGenBuffers(1, &bufferPlatParams->m_bufferName);

			bufferPlatParams->m_baseOffset = 0; // Permanent buffers have their own dedicated buffers

			GLenum bufferTarget = 0;
			switch (bufferPlatParams->m_dataType)
			{
			case re::Buffer::DataType::SingleElement:
			{
				bufferTarget = GL_UNIFORM_BUFFER;
			}
			break;
			case re::Buffer::DataType::Array:
			{
				bufferTarget = GL_SHADER_STORAGE_BUFFER;
			}
			break;
			default: SEAssertF("Invalid DataType");
			}

			// Binding associates the buffer object with the buffer object name
			glBindBuffer(bufferTarget, bufferPlatParams->m_bufferName);
			SEAssert(glIsBuffer(bufferPlatParams->m_bufferName) == GL_TRUE, "Failed to generate buffer object");

			glBufferData(
				bufferTarget,
				(GLsizeiptr)numBytes,
				nullptr, // NULL: Data store of the specified size is created, but remains uninitialized and thus undefined
				bufferType == re::Buffer::Type::Immutable ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);

			// RenderDoc label:
			std::string const& bufferName = buffer.GetName() + "_Immutable";
			glObjectLabel(GL_BUFFER, bufferPlatParams->m_bufferName, -1, bufferName.c_str());
		}
		break;
		case re::Buffer::Type::SingleFrame:
		{
			opengl::BufferAllocator::GetSubAllocation(
				bufferPlatParams->m_dataType,
				numBytes,
				bufferPlatParams->m_bufferName,
				bufferPlatParams->m_baseOffset);
		}
		break;
		default: SEAssertF("Invalid Type");
		}
	}


	void Buffer::Update(
		re::Buffer const& buffer, uint8_t curFrameHeapOffsetFactor, uint32_t baseOffset, uint32_t numBytes)
	{
		// Note: OpenGL manages heap synchronization for us, so we don't need to manually manage mutable buffers of
		// size * numFramesInFlight bytes. Thus, curFrameHeapOffsetFactor is unused here.

		PlatformParams* bufferPlatParams = buffer.GetPlatformParams()->As<opengl::Buffer::PlatformParams*>();

		void const* data;
		uint32_t totalBytes;
		buffer.GetDataAndSize(data, totalBytes);

		//glNamedBufferSubData(
		//	bufferPlatParams->m_bufferName,	// Target
		//	bufferPlatParams->m_baseOffset,	// Offset
		//	(GLsizeiptr)totalBytes,		// Size
		//	data);						// Data

		const GLbitfield access = GL_MAP_WRITE_BIT;

		const bool updateAllBytes = baseOffset == 0 && (numBytes == 0 || numBytes == totalBytes);

		SEAssert(updateAllBytes ||
			(baseOffset + numBytes <= totalBytes),
			"Base offset and number of bytes are out of bounds");

		// Adjust our source pointer if we're doing a partial update:
		if (!updateAllBytes)
		{
			SEAssert(buffer.GetType() == re::Buffer::Type::Mutable,
				"Only mutable buffers can be partially updated");

			// Update the source data pointer:
			data = static_cast<uint8_t const*>(data) + baseOffset;
			totalBytes = numBytes;
		}

		// Map and copy the data:
		void* cpuVisibleData = glMapNamedBufferRange(
			bufferPlatParams->m_bufferName,
			bufferPlatParams->m_baseOffset + baseOffset,
			(GLsizeiptr)totalBytes,
			access);

		memcpy(cpuVisibleData, data, totalBytes);

		glUnmapNamedBuffer(bufferPlatParams->m_bufferName);
	}


	void Buffer::Destroy(re::Buffer& buffer)
	{
		PlatformParams* bufferPlatParams = buffer.GetPlatformParams()->As<opengl::Buffer::PlatformParams*>();
		SEAssert(bufferPlatParams->m_isCreated, "Attempting to destroy a Buffer that has not been created");

		const re::Buffer::Type bufferType = buffer.GetType();
		switch (bufferType)
		{
		case re::Buffer::Type::Mutable:
		case re::Buffer::Type::Immutable:
		{
			glDeleteBuffers(1, &bufferPlatParams->m_bufferName);
		}
		break;
		case re::Buffer::Type::SingleFrame:
		{
			// Do nothing: Buffer allocator is responsible for destroying the shared buffers
		}
		break;
		default: SEAssertF("Invalid Type");
		}

		bufferPlatParams->m_bufferName = 0;
		bufferPlatParams->m_baseOffset = 0;
		bufferPlatParams->m_isCreated = false;
	}


	void Buffer::Bind(re::Buffer const& buffer, GLuint bindIndex)
	{
		PlatformParams* bufferPlatParams = buffer.GetPlatformParams()->As<opengl::Buffer::PlatformParams*>();
		 
		GLenum bufferTarget = 0;
		switch (bufferPlatParams->m_dataType)
		{
		case re::Buffer::DataType::SingleElement:
		{
			bufferTarget = GL_UNIFORM_BUFFER;
		}
		break;
		case re::Buffer::DataType::Array:
		{
			bufferTarget = GL_SHADER_STORAGE_BUFFER;
		}
		break;
		default: SEAssertF("Invalid DataType");
		}

		void const* data;
		uint32_t numBytes;
		buffer.GetDataAndSize(data, numBytes);
		glBindBufferRange(bufferTarget, bindIndex, bufferPlatParams->m_bufferName, bufferPlatParams->m_baseOffset, numBytes);
	}
}