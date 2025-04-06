// © 2022 Adam Badke. All rights reserved.
#include "Buffer_OpenGL.h"
#include "Buffer.h"
#include "BufferAllocator_OpenGL.h"
#include "BufferView.h"
#include "Context.h"

#include "Core/Assert.h"


namespace opengl
{
	void Buffer::PlatformParams::Destroy()
	{
		SEAssert(m_isCreated, "Attempting to destroy a Buffer that has not been created");

		if (!m_isSharedBufferName &&
			m_bufferName != 0)
		{
			glDeleteBuffers(1, &m_bufferName);
			m_bufferName = 0;
		}

		m_baseOffset = 0;
		m_isCreated = false;
	}


	void Buffer::Create(re::Buffer& buffer)
	{
		SEAssert(!re::Buffer::HasUsageBit(re::Buffer::Constant, buffer.GetBufferParams()) ||
			buffer.GetBufferParams().m_arraySize == 1,
			"TODO: Support Constant buffer arrays. Previously, we only allowed single element Constant buffers "
			"and arrays were achieved as an array member variable within the buffer. This restriction was removed for "
			"DX12 bindless resources, if you hit this we now need to solve this usage pattern for OpenGL buffers");

		PlatformParams* bufferPlatParams = buffer.GetPlatformParams()->As<opengl::Buffer::PlatformParams*>();
		SEAssert(!bufferPlatParams->m_isCreated, "Buffer is already created");
		bufferPlatParams->m_isCreated = true;

		void const* data;
		uint32_t numBytes;
		buffer.GetDataAndSize(&data, &numBytes);

		const re::Buffer::StagingPool bufferAlloc = buffer.GetStagingPool();
		const re::Lifetime bufferLifetime = buffer.GetLifetime();
		switch (bufferLifetime)
		{
		case re::Lifetime::Permanent:
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
				bufferAlloc == re::Buffer::StagingPool::Permanent ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);

			// RenderDoc label:
			std::string const& bufferName =
				buffer.GetName() + (bufferAlloc == re::Buffer::StagingPool::Permanent ? "_CPUMutable" : "_CPUImmutable");
			glObjectLabel(GL_BUFFER, bufferPlatParams->m_bufferName, -1, bufferName.c_str());
		}
		break;
		case re::Lifetime::SingleFrame:
		{
			opengl::BufferAllocator* bufferAllocator =
				dynamic_cast<opengl::BufferAllocator*>(re::Context::Get()->GetBufferAllocator());

			bufferAllocator->GetSubAllocation(
				buffer.GetUsageMask(),
				numBytes,
				bufferPlatParams->m_bufferName,
				bufferPlatParams->m_baseOffset);

			bufferPlatParams->m_isSharedBufferName = true;
		}
		break;
		default: SEAssertF("Invalid lifetime");
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
		case re::Buffer::DefaultHeap:
		{
			glNamedBufferSubData(
				bufferPlatParams->m_bufferName,			// Target
				bufferPlatParams->m_baseOffset,			// Offset
				static_cast<GLsizeiptr>(totalBytes),	// Size
				data);									// Data
		}
		break;
		case re::Buffer::UploadHeap:
		{
			const GLbitfield access = GL_MAP_WRITE_BIT;

			const bool updateAllBytes = baseOffset == 0 && (numBytes == 0 || numBytes == totalBytes);

			SEAssert(updateAllBytes ||
				(baseOffset + numBytes <= totalBytes),
				"Base offset and number of bytes are out of bounds");

			// Adjust our source pointer if we're doing a partial update:
			if (!updateAllBytes)
			{
				SEAssert(buffer.GetStagingPool() == re::Buffer::StagingPool::Permanent,
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


	void Buffer::Bind(re::Buffer const& buffer, BindTarget bindTarget, re::BufferView const& view, GLuint bindIndex)
	{
		const uint32_t numBytes = buffer.GetTotalBytes();

		PlatformParams const* bufferPlatParams = buffer.GetPlatformParams()->As<opengl::Buffer::PlatformParams const*>();

		switch (bindTarget)
		{
		case BindTarget::UBO:
		{
			SEAssert(re::Buffer::HasUsageBit(re::Buffer::Constant, buffer),
				"Buffer is missing the Constant usage bit");

			glBindBufferRange(GL_UNIFORM_BUFFER,
				bindIndex,
				bufferPlatParams->m_bufferName,
				bufferPlatParams->m_baseOffset,
				numBytes);
		}
		break;
		case BindTarget::SSBO:
		{
			SEAssert(re::Buffer::HasUsageBit(re::Buffer::Structured, buffer),
				"Buffer is missing the Structured usage bit");

			glBindBufferRange(GL_SHADER_STORAGE_BUFFER,
				bindIndex,
				bufferPlatParams->m_bufferName,
				bufferPlatParams->m_baseOffset,
				numBytes);
		}
		break;
		case BindTarget::Vertex:
		{
			SEAssert(re::Buffer::HasUsageBit(re::Buffer::Raw, buffer),
				"Buffer is missing the VertexStream usage bit");

			glBindVertexBuffer(
				bindIndex,											// Slot index
				bufferPlatParams->m_bufferName,						// Buffer
				bufferPlatParams->m_baseOffset,						// Offset
				DataTypeToByteStride(view.m_stream.m_dataType));	// Stride
		}
		break;
		case BindTarget::Index:
		{
			SEAssert(re::Buffer::HasUsageBit(re::Buffer::Raw, buffer),
				"Buffer is missing the VertexStream usage bit");

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferPlatParams->m_bufferName);
		}
		break;
		default: SEAssertF("Invalid view type");
		}
	}


	void const* Buffer::MapCPUReadback(re::Buffer const& buffer, uint8_t frameLatency)
	{
		const uint32_t bufferSize = buffer.GetTotalBytes();

		PlatformParams* bufferPlatParams = buffer.GetPlatformParams()->As<opengl::Buffer::PlatformParams*>();

		void* cpuVisibleData = glMapNamedBufferRange(
			bufferPlatParams->m_bufferName,
			bufferPlatParams->m_baseOffset,	// offset
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