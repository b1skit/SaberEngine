// © 2022 Adam Badke. All rights reserved.
#include "Buffer_OpenGL.h"
#include "Buffer.h"
#include "BufferAllocator_OpenGL.h"
#include "BufferView.h"
#include "Context.h"
#include "EnumTypes.h"
#include "RenderManager.h"

#include "Core/Assert.h"


namespace opengl
{
	void Buffer::PlatObj::Destroy()
	{
		SEAssert(m_isCreated, "Attempting to destroy a Buffer that has not been created");

		if (!m_isSharedBufferName &&
			m_bufferName != 0)
		{
			glDeleteBuffers(1, &m_bufferName);
			m_bufferName = 0;
		}

		m_baseByteOffset = 0;
		m_isCreated = false;
	}


	void Buffer::Create(re::Buffer& buffer, re::IBufferAllocatorAccess*, uint8_t numFramesInFlight)
	{
		SEAssert(!re::Buffer::HasUsageBit(re::Buffer::Constant, buffer.GetBufferParams()) ||
			buffer.GetBufferParams().m_arraySize == 1,
			"TODO: Support Constant buffer arrays. Previously, we only allowed single element Constant buffers "
			"and arrays were achieved as an array member variable within the buffer. This restriction was removed for "
			"DX12 bindless resources, if you hit this we now need to solve this usage pattern for OpenGL buffers");

		PlatObj* bufferPlatObj = buffer.GetPlatformObject()->As<opengl::Buffer::PlatObj*>();
		SEAssert(!bufferPlatObj->m_isCreated, "Buffer is already created");
		bufferPlatObj->m_isCreated = true;

		void const* data;
		uint32_t numBytes;
		buffer.GetDataAndSize(&data, &numBytes);

		const re::Buffer::StagingPool stagingPool = buffer.GetStagingPool();
		const re::Lifetime bufferLifetime = buffer.GetLifetime();
		switch (bufferLifetime)
		{
		case re::Lifetime::Permanent:
		{
			// Note: Unlike DX12, OpenGL handles buffer synchronization for us (so long as they're not persistently 
			// mapped). So we can just create a single mutable buffer and write to it as needed, instead of 
			// sub-allocating from within a larger buffer each frame

			// Generate the buffer name:
			glCreateBuffers(1, &bufferPlatObj->m_bufferName);

			bufferPlatObj->m_baseByteOffset = 0; // Permanent buffers have their own dedicated buffers

			// Create the data store (contents remain uninitialized/undefined):
			glNamedBufferData(
				bufferPlatObj->m_bufferName,
				static_cast<GLsizeiptr>(numBytes),
				nullptr,
				stagingPool == re::Buffer::StagingPool::Permanent ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

			// RenderDoc label:
			std::string const& bufferName =
				buffer.GetName() + (stagingPool == re::Buffer::StagingPool::Permanent ? "_CPUMutable" : "_CPUImmutable");
			glObjectLabel(GL_BUFFER, bufferPlatObj->m_bufferName, -1, bufferName.c_str());
		}
		break;
		case re::Lifetime::SingleFrame:
		{
			opengl::BufferAllocator* bufferAllocator =
				dynamic_cast<opengl::BufferAllocator*>(gr::RenderManager::Get()->GetContext()->GetBufferAllocator());

			bufferAllocator->GetSubAllocation(
				buffer.GetUsageMask(),
				numBytes,
				bufferPlatObj->m_bufferName,
				bufferPlatObj->m_baseByteOffset);

			bufferPlatObj->m_isSharedBufferName = true;
		}
		break;
		default: SEAssertF("Invalid lifetime");
		}
	}


	void Buffer::Update(
		re::Buffer const& buffer, uint8_t frameOffsetIdx_UNUSED, uint32_t commitBaseOffset, uint32_t numBytes)
	{
		SEAssert(numBytes > 0, "Invalid update size");

		// Note: OpenGL manages heap synchronization for us, so we don't need to manually manage mutable buffers of
		// size * numFramesInFlight bytes. Thus, frameOffsetIdx_UNUSED is unused here.

		PlatObj const* bufferPlatObj = buffer.GetPlatformObject()->As<opengl::Buffer::PlatObj const*>();

		void const* srcData;
		uint32_t totalBytes;
		buffer.GetDataAndSize(&srcData, &totalBytes);

		// Update the source data pointer:
		srcData = static_cast<uint8_t const*>(srcData) + commitBaseOffset;

		switch (buffer.GetBufferParams().m_memPoolPreference)
		{
		case re::Buffer::DefaultHeap:
		{
			glNamedBufferSubData(
				bufferPlatObj->m_bufferName,						// Target
				bufferPlatObj->m_baseByteOffset + commitBaseOffset,	// Offset
				static_cast<GLsizeiptr>(numBytes),					// Size
				srcData);											// Data
		}
		break;
		case re::Buffer::UploadHeap:
		{
			SEAssert(commitBaseOffset + numBytes <= totalBytes,
				"Base offset and number of bytes are out of bounds");

			SEAssert(buffer.GetStagingPool() == re::Buffer::StagingPool::Permanent ||
				commitBaseOffset == 0 && numBytes == totalBytes,
				"Only mutable buffers can be partially updated");

			const GLbitfield access = GL_MAP_WRITE_BIT;

			// Map and copy the data:
			void* cpuVisibleData = glMapNamedBufferRange(
				bufferPlatObj->m_bufferName,						// buffer
				bufferPlatObj->m_baseByteOffset + commitBaseOffset,	// offset
				static_cast<GLsizeiptr>(numBytes),					// length
				access);											// access

			memcpy(cpuVisibleData, srcData, numBytes);

			glUnmapNamedBuffer(bufferPlatObj->m_bufferName);
		}
		break;
		default: SEAssertF("Invalid MemoryPoolPreference");
		}
	}


	void Buffer::Bind(re::Buffer const& buffer, BindTarget bindTarget, re::BufferView const& view, GLuint bindIndex)
	{
		const uint32_t numBytes = buffer.GetTotalBytes();

		PlatObj const* bufferPlatObj = buffer.GetPlatformObject()->As<opengl::Buffer::PlatObj const*>();

		// Compute an additional offset for buffer views with a non-zero first element offset
		const uint32_t alignedSize = 
			opengl::BufferAllocator::GetAlignedSize(numBytes, buffer.GetBufferParams().m_usageMask);
		
		switch (bindTarget)
		{
		case BindTarget::UBO:
		{
			SEAssert(re::Buffer::HasUsageBit(re::Buffer::Constant, buffer),
				"Buffer is missing the Constant usage bit");

			const uint32_t viewByteOffset = alignedSize * view.m_bufferView.m_firstElement;

			glBindBufferRange(GL_UNIFORM_BUFFER,
				bindIndex,
				bufferPlatObj->m_bufferName,
				bufferPlatObj->m_baseByteOffset + viewByteOffset,
				numBytes);
		}
		break;
		case BindTarget::SSBO:
		{
			SEAssert(re::Buffer::HasUsageBit(re::Buffer::Structured, buffer),
				"Buffer is missing the Structured usage bit");
		
			const uint32_t viewByteOffset = view.m_bufferView.m_structuredByteStride * view.m_bufferView.m_firstElement;

			glBindBufferRange(GL_SHADER_STORAGE_BUFFER,
				bindIndex,
				bufferPlatObj->m_bufferName,
				bufferPlatObj->m_baseByteOffset + viewByteOffset,
				numBytes);
		}
		break;
		case BindTarget::Vertex:
		{
			SEAssert(re::Buffer::HasUsageBit(re::Buffer::Raw, buffer),
				"Buffer is missing the VertexStream usage bit");

			const uint32_t viewByteOffset = alignedSize * view.m_streamView.m_firstElement;

			glBindVertexBuffer(
				bindIndex,												// Slot index
				bufferPlatObj->m_bufferName,							// Buffer
				bufferPlatObj->m_baseByteOffset + viewByteOffset,		// Offset
				DataTypeToByteStride(view.m_streamView.m_dataType));	// Stride
		}
		break;
		case BindTarget::Index:
		{
			SEAssert(re::Buffer::HasUsageBit(re::Buffer::Raw, buffer),
				"Buffer is missing the VertexStream usage bit");

			SEAssert(view.m_streamView.m_firstElement == 0, "TODO: Support binding subranges within index streams");

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferPlatObj->m_bufferName);
		}
		break;
		default: SEAssertF("Invalid view type");
		}
	}


	void const* Buffer::MapCPUReadback(re::Buffer const& buffer, re::IBufferAllocatorAccess const*, uint8_t frameLatency)
	{
		const uint32_t bufferSize = buffer.GetTotalBytes();

		PlatObj* bufferPlatObj = buffer.GetPlatformObject()->As<opengl::Buffer::PlatObj*>();

		void* cpuVisibleData = glMapNamedBufferRange(
			bufferPlatObj->m_bufferName,
			bufferPlatObj->m_baseByteOffset,// offset
			(GLsizeiptr)bufferSize,			// length
			GL_MAP_READ_BIT);				// access

		return cpuVisibleData;
	}


	void Buffer::UnmapCPUReadback(re::Buffer const& buffer, re::IBufferAllocatorAccess const*)
	{
		PlatObj* bufferPlatObj = buffer.GetPlatformObject()->As<opengl::Buffer::PlatObj*>();

		glUnmapNamedBuffer(bufferPlatObj->m_bufferName);
	}
}