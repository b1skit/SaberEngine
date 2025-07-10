// © 2023 Adam Badke. All rights reserved.
#include "Buffer_OpenGL.h"
#include "BufferAllocator.h"
#include "BufferAllocator_OpenGL.h"
#include "SysInfo_OpenGL.h"

#include "Core/Util/MathUtils.h"


namespace opengl
{
	uint32_t BufferAllocator::GetAlignedSize(uint32_t bufferByteSize, re::Buffer::Usage usageMask)
	{
		const re::BufferAllocator::AllocationPool allocationPool =
			re::BufferAllocator::BufferUsageMaskToAllocationPool(usageMask);

		switch (allocationPool)
		{
		case re::BufferAllocator::Constant:
		{
			const GLint uboAlignment = opengl::SysInfo::GetUniformBufferOffsetAlignment(); // e.g. 256
			SEAssert(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize % uboAlignment == 0,
				"Incompatible alignment");

			return util::RoundUpToNearestMultiple<uint32_t>(bufferByteSize, uboAlignment);
		}
		break;
		case re::BufferAllocator::Structured:
		{
			const GLint ssboAlignment = opengl::SysInfo::GetShaderStorageBufferOffsetAlignment(); // e.g. 16
			SEAssert(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize % ssboAlignment == 0,
				"Incompatible alignment");

			return util::RoundUpToNearestMultiple<uint32_t>(bufferByteSize, ssboAlignment);
		}
		break;
		case re::BufferAllocator::Raw:
		{
			constexpr GLint k_vertexAlignment = 16; // Minimum alignment of a float4 is 16B

			return util::RoundUpToNearestMultiple<uint32_t>(bufferByteSize, k_vertexAlignment);
		}
		break;
		default: SEAssertF("Invalid AllocationPool");
		}
		return 0; // This should never happen
	}

	
	void BufferAllocator::GetSubAllocation(
		re::Buffer::Usage usageMask, 
		uint32_t size, 
		GLuint& bufferNameOut,
		GLintptr& baseOffsetOut)
	{
		const uint8_t writeIdx = GetSingleFrameGPUWriteIndex();

		const uint32_t alignedSize = GetAlignedSize(size, usageMask);

		const re::BufferAllocator::AllocationPool allocationPool = 
			re::BufferAllocator::BufferUsageMaskToAllocationPool(usageMask);

		SEAssert(allocationPool != re::BufferAllocator::Constant || size <= 4096 * sizeof(glm::vec4),
			"Constant buffers can only hold up to 4096 float4's");

		switch (allocationPool)
		{
		case re::BufferAllocator::Constant:
		{
			bufferNameOut = m_singleFrameBuffers[re::BufferAllocator::Constant][writeIdx];

			const GLint uboAlignment = opengl::SysInfo::GetUniformBufferOffsetAlignment(); // e.g. 256
			SEAssert(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize % uboAlignment == 0,
				"Incompatible alignment");
		}
		break;
		case re::BufferAllocator::Structured:
		{
			bufferNameOut = m_singleFrameBuffers[re::BufferAllocator::Structured][writeIdx];

			const GLint ssboAlignment = opengl::SysInfo::GetShaderStorageBufferOffsetAlignment(); // e.g. 16
			SEAssert(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize % ssboAlignment == 0,
				"Incompatible alignment");
		}
		break;
		case re::BufferAllocator::Raw:
		{
			bufferNameOut = m_singleFrameBuffers[re::BufferAllocator::Raw][writeIdx];
		}
		break;
		default: SEAssertF("Invalid AllocationPool");
		}

		baseOffsetOut = AdvanceBaseIdx(allocationPool, alignedSize);
	}


	void BufferAllocator::InitializeInternal(uint64_t currentFrame, void* unused)
	{
		// Note: OpenGL only supports double-buffering via a front and back buffer. Thus we can fill one buffer while
		// the other is in use, so long as we clear the buffer we're writing to at the beginning of each new frame
		
		for (uint8_t i = 0; i < re::BufferAllocator::AllocationPool_Count; ++i)
		{
			m_singleFrameBuffers[i].resize(m_numFramesInFlight, 0);

			// Generate all of our buffer names for each frame at once:
			glCreateBuffers(m_numFramesInFlight, m_singleFrameBuffers[i].data());
		}
		
		
		for (uint8_t bufferIdx = 0; bufferIdx < m_numFramesInFlight; bufferIdx++)
		{
			// UBO:
			SEAssert(glIsBuffer(m_singleFrameBuffers[re::BufferAllocator::Constant][bufferIdx]),
				"Buffer name is not valid");

			glNamedBufferData(
				m_singleFrameBuffers[re::BufferAllocator::Constant][bufferIdx],
				static_cast<GLsizeiptr>(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize),
				nullptr,
				GL_DYNAMIC_DRAW);

			glObjectLabel(GL_BUFFER, 
				m_singleFrameBuffers[re::BufferAllocator::Constant][bufferIdx],
				-1, 
				std::format("Single-frame shared UBO {}", bufferIdx).c_str());


			// SSBO:
			SEAssert(glIsBuffer(m_singleFrameBuffers[re::BufferAllocator::Structured][bufferIdx]),
				"Buffer name is not valid");

			glNamedBufferData(
				m_singleFrameBuffers[re::BufferAllocator::Structured][bufferIdx],
				static_cast<GLsizeiptr>(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize),
				nullptr,
				GL_DYNAMIC_DRAW);

			glObjectLabel(GL_BUFFER,
				m_singleFrameBuffers[re::BufferAllocator::Structured][bufferIdx],
				-1,
				std::format("Single-frame shared SSBO {}", bufferIdx).c_str());

			// VertexStream:
			SEAssert(glIsBuffer(m_singleFrameBuffers[re::BufferAllocator::Raw][bufferIdx]),
				"Buffer name is not valid");

			glNamedBufferData(
				m_singleFrameBuffers[re::BufferAllocator::Raw][bufferIdx],
				static_cast<GLsizeiptr>(re::BufferAllocator::k_sharedSingleFrameAllocationByteSize),
				nullptr,
				GL_DYNAMIC_DRAW);

			glObjectLabel(GL_BUFFER,
				m_singleFrameBuffers[re::BufferAllocator::Raw][bufferIdx],
				-1,
				std::format("Single-frame shared SSBO {}", bufferIdx).c_str());
		}
	}


	void BufferAllocator::BufferDefaultHeapDataPlatform(
		std::vector<PlatformCommitMetadata> const& dirtyBuffersForPlatformUpdate,
		uint8_t frameOffsetIdx)
	{	
		for (auto const& entry : dirtyBuffersForPlatformUpdate)
		{
			// OpenGL allows buffers to be updated via a CPU-side map, regardless of where the actual resource data is
			// held in memory. So we just forward our buffers on to the standard update function here
			opengl::Buffer::Update(*entry.m_buffer, 0, entry.m_baseOffset, entry.m_numBytes);
		}
	}


	void BufferAllocator::Destroy()
	{
		for (uint8_t i = 0; i < re::BufferAllocator::AllocationPool_Count; ++i)
		{
			glDeleteBuffers(m_numFramesInFlight, m_singleFrameBuffers[i].data());

			m_singleFrameBuffers[i].assign(m_numFramesInFlight, 0);
		}	

		re::BufferAllocator::Destroy();
	}
}