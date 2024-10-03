// © 2023 Adam Badke. All rights reserved.
#include "Buffer_OpenGL.h"
#include "Context.h"
#include "Core/Util/MathUtils.h"
#include "BufferAllocator.h"
#include "BufferAllocator_OpenGL.h"
#include "RenderManager_OpenGL.h"
#include "SysInfo_OpenGL.h"


namespace opengl
{
	void BufferAllocator::GetSubAllocation(
		re::Buffer::Type dataType, 
		uint32_t size, 
		GLuint& bufferNameOut,
		GLintptr& baseOffsetOut)
	{
		const uint8_t writeIdx = GetWriteIndex();

		uint32_t alignedSize = 0;
		switch (dataType)
		{
		case re::Buffer::Type::Constant:
		{
			bufferNameOut = m_singleFrameBuffers[re::Buffer::Constant][writeIdx];

			const GLint uboAlignment = opengl::SysInfo::GetUniformBufferOffsetAlignment(); // e.g. 256
			SEAssert(re::BufferAllocator::k_fixedAllocationByteSize % uboAlignment == 0,
				"Incompatible alignment");

			alignedSize = util::RoundUpToNearestMultiple<uint32_t>(size, uboAlignment);
		}
		break;
		case re::Buffer::Type::Structured:
		{
			bufferNameOut = m_singleFrameBuffers[re::Buffer::Structured][writeIdx];

			const GLint ssboAlignment = opengl::SysInfo::GetShaderStorageBufferOffsetAlignment(); // e.g. 16
			SEAssert(re::BufferAllocator::k_fixedAllocationByteSize % ssboAlignment == 0,
				"Incompatible alignment");

			alignedSize = util::RoundUpToNearestMultiple<uint32_t>(size, ssboAlignment);
		}
		break;
		case re::Buffer::Type::VertexStream:
		{
			bufferNameOut = m_singleFrameBuffers[re::Buffer::VertexStream][writeIdx];

			constexpr GLint vertexAlignment = 16; // Minimum alignment of a float4 is 16B

			alignedSize = util::RoundUpToNearestMultiple<uint32_t>(size, vertexAlignment);
		}
		break;
		default: SEAssertF("Invalid Type");
		}

		baseOffsetOut = AdvanceBaseIdx(dataType, alignedSize);
	}


	void BufferAllocator::Initialize(uint64_t currentFrame)
	{
		re::BufferAllocator::Initialize(currentFrame);

		// Note: OpenGL only supports double-buffering via a front and back buffer. Thus we can fill one buffer while
		// the other is in use, so long as we clear the buffer we're writing to at the beginning of each new frame
		
		for (uint8_t i = 0; i < re::Buffer::Type::Type_Count; ++i)
		{
			m_singleFrameBuffers[i].resize(m_numFramesInFlight, 0);

			// Generate all of our buffer names for each frame at once:
			glCreateBuffers(m_numFramesInFlight, m_singleFrameBuffers[i].data());
		}
		
		
		for (uint8_t bufferIdx = 0; bufferIdx < m_numFramesInFlight; bufferIdx++)
		{
			// UBO:
			SEAssert(glIsBuffer(m_singleFrameBuffers[re::Buffer::Constant][bufferIdx]), "Buffer name is not valid");

			glNamedBufferData(
				m_singleFrameBuffers[re::Buffer::Constant][bufferIdx],
				static_cast<GLsizeiptr>(re::BufferAllocator::k_fixedAllocationByteSize),
				nullptr,
				GL_DYNAMIC_DRAW);

			glObjectLabel(GL_BUFFER, 
				m_singleFrameBuffers[re::Buffer::Constant][bufferIdx],
				-1, 
				std::format("Single-frame shared UBO {}", bufferIdx).c_str());


			// SSBO:
			SEAssert(glIsBuffer(m_singleFrameBuffers[re::Buffer::Structured][bufferIdx]), "Buffer name is not valid");

			glNamedBufferData(
				m_singleFrameBuffers[re::Buffer::Structured][bufferIdx],
				static_cast<GLsizeiptr>(re::BufferAllocator::k_fixedAllocationByteSize),
				nullptr,
				GL_DYNAMIC_DRAW);

			glObjectLabel(GL_BUFFER,
				m_singleFrameBuffers[re::Buffer::Structured][bufferIdx],
				-1,
				std::format("Single-frame shared SSBO {}", bufferIdx).c_str());

			// VertexStream:
			SEAssert(glIsBuffer(m_singleFrameBuffers[re::Buffer::VertexStream][bufferIdx]), "Buffer name is not valid");

			glNamedBufferData(
				m_singleFrameBuffers[re::Buffer::VertexStream][bufferIdx],
				static_cast<GLsizeiptr>(re::BufferAllocator::k_fixedAllocationByteSize),
				nullptr,
				GL_DYNAMIC_DRAW);

			glObjectLabel(GL_BUFFER,
				m_singleFrameBuffers[re::Buffer::VertexStream][bufferIdx],
				-1,
				std::format("Single-frame shared SSBO {}", bufferIdx).c_str());
		}
	}


	void BufferAllocator::BufferDataPlatform()
	{
		// Note: BufferAllocator::m_dirtyBuffersForPlatformUpdateMutex is already locked by this point
		
		for (auto const& entry : m_dirtyBuffersForPlatformUpdate)
		{
			// OpenGL allows buffers to be updated via a CPU-side map, regardless of where the actual resource data is
			// held in memory. So we just forward our buffers on to the standard update function here
			opengl::Buffer::Update(*entry.m_buffer, 0, entry.m_baseOffset, entry.m_numBytes);
		}
	}


	void BufferAllocator::Destroy()
	{
		for (uint8_t i = 0; i < re::Buffer::Type::Type_Count; ++i)
		{
			glDeleteBuffers(m_numFramesInFlight, m_singleFrameBuffers[i].data());

			m_singleFrameBuffers[i].assign(m_numFramesInFlight, 0);
		}	

		re::BufferAllocator::Destroy();
	}
}