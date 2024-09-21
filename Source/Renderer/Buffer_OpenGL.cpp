// © 2022 Adam Badke. All rights reserved.
#include "Core/Assert.h"
#include "Context.h"
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
		buffer.GetDataAndSize(&data, &numBytes);

		const re::Buffer::CPUAllocation bufferAlloc = buffer.GetCPUAllocationType();
		switch (bufferAlloc)
		{
		case re::Buffer::CPUAllocation::Mutable:
		case re::Buffer::CPUAllocation::Immutable:
		{
			// Note: Unlike DX12, OpenGL handles buffer synchronization for us (so long as they're not persistently 
			// mapped). So we can just create a single mutable buffer and write to it as needed, instead of 
			// sub-allocating from within a larger buffer each frame

			// Generate the buffer name:
			glCreateBuffers(1, &bufferPlatParams->m_bufferName);

			bufferPlatParams->m_baseOffset = 0; // Permanent buffers have their own dedicated buffers

			// Create the data store (contents remain uninitialized/undefined):
			glNamedBufferData(
				bufferPlatParams->m_bufferName,
				static_cast<GLsizeiptr>(numBytes),
				nullptr,
				bufferAlloc == re::Buffer::CPUAllocation::Mutable ? GL_DYNAMIC_DRAW  : GL_STATIC_DRAW);

			// RenderDoc label:
			std::string const& bufferName = 
				buffer.GetName() + (bufferAlloc == re::Buffer::CPUAllocation::Mutable ? "_Mutable" : "_Immutable");
			glObjectLabel(GL_BUFFER, bufferPlatParams->m_bufferName, -1, bufferName.c_str());
		}
		break;
		case re::Buffer::CPUAllocation::SingleFrame:
		{
			opengl::BufferAllocator* bufferAllocator =
				dynamic_cast<opengl::BufferAllocator*>(re::Context::Get()->GetBufferAllocator());

			bufferAllocator->GetSubAllocation(
				buffer.GetBufferParams().m_type,
				numBytes,
				bufferPlatParams->m_bufferName,
				bufferPlatParams->m_baseOffset);
		}
		break;
		default: SEAssertF("Invalid CPUAllocation");
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
		buffer.GetDataAndSize(&data, &totalBytes);

		switch (buffer.GetBufferParams().m_memPoolPreference)
		{
		case re::Buffer::MemoryPoolPreference::Default:
		{
			glNamedBufferSubData(
				bufferPlatParams->m_bufferName,			// Target
				bufferPlatParams->m_baseOffset,			// Offset
				static_cast<GLsizeiptr>(totalBytes),	// Size
				data);									// Data
		}
		break;
		case re::Buffer::MemoryPoolPreference::Upload:
		{
			const GLbitfield access = GL_MAP_WRITE_BIT;

			const bool updateAllBytes = baseOffset == 0 && (numBytes == 0 || numBytes == totalBytes);

			SEAssert(updateAllBytes ||
				(baseOffset + numBytes <= totalBytes),
				"Base offset and number of bytes are out of bounds");

			// Adjust our source pointer if we're doing a partial update:
			if (!updateAllBytes)
			{
				SEAssert(buffer.GetCPUAllocationType() == re::Buffer::CPUAllocation::Mutable,
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
		break;
		default: SEAssertF("Invalid MemoryPoolPreference");
		}
	}


	void Buffer::Destroy(re::Buffer& buffer)
	{
		PlatformParams* bufferPlatParams = buffer.GetPlatformParams()->As<opengl::Buffer::PlatformParams*>();
		SEAssert(bufferPlatParams->m_isCreated, "Attempting to destroy a Buffer that has not been created");

		const re::Buffer::CPUAllocation bufferAlloc = buffer.GetCPUAllocationType();
		switch (bufferAlloc)
		{
		case re::Buffer::CPUAllocation::Mutable:
		case re::Buffer::CPUAllocation::Immutable:
		{
			glDeleteBuffers(1, &bufferPlatParams->m_bufferName);
		}
		break;
		case re::Buffer::CPUAllocation::SingleFrame:
		{
			// Do nothing: Buffer allocator is responsible for destroying the shared buffers
		}
		break;
		default: SEAssertF("Invalid CPUAllocation");
		}

		bufferPlatParams->m_bufferName = 0;
		bufferPlatParams->m_baseOffset = 0;
		bufferPlatParams->m_isCreated = false;
	}


	void Buffer::Bind(re::Buffer const& buffer, GLuint bindIndex)
	{
		PlatformParams* bufferPlatParams = buffer.GetPlatformParams()->As<opengl::Buffer::PlatformParams*>();
		 
		GLenum bufferTarget = 0;
		switch (buffer.GetBufferParams().m_type)
		{
		case re::Buffer::Type::Constant:
		{
			bufferTarget = GL_UNIFORM_BUFFER;
		}
		break;
		case re::Buffer::Type::Structured:
		{
			bufferTarget = GL_SHADER_STORAGE_BUFFER;
		}
		break;
		default: SEAssertF("Invalid Type");
		}

		const uint32_t numBytes = buffer.GetSize();
		glBindBufferRange(
			bufferTarget, bindIndex, bufferPlatParams->m_bufferName, bufferPlatParams->m_baseOffset, numBytes);
	}


	void const* Buffer::MapCPUReadback(re::Buffer const& buffer, uint8_t frameLatency)
	{
		const uint32_t bufferSize = buffer.GetSize();

		PlatformParams* bufferPlatParams = buffer.GetPlatformParams()->As<opengl::Buffer::PlatformParams*>();

		void* cpuVisibleData = glMapNamedBufferRange(
			bufferPlatParams->m_bufferName,
			0,								// offset
			(GLsizeiptr)bufferSize,			// length
			GL_MAP_READ_BIT);				// access

		return cpuVisibleData;
	}


	void Buffer::UnmapCPUReadback(re::Buffer const& buffer)
	{
		PlatformParams* bufferPlatParams = buffer.GetPlatformParams()->As<opengl::Buffer::PlatformParams*>();

		glUnmapNamedBuffer(bufferPlatParams->m_bufferName);
	}
}