// � 2023 Adam Badke. All rights reserved.
#include "Buffer_OpenGL.h"
#include "Context.h"
#include "MathUtils.h"
#include "BufferAllocator.h"
#include "BufferAllocator_OpenGL.h"
#include "RenderManager_OpenGL.h"
#include "SysInfo_OpenGL.h"


namespace opengl
{
	void BufferAllocator::GetSubAllocation(
		re::Buffer::DataType dataType, 
		uint32_t size, 
		GLuint& bufferNameOut,
		GLintptr& baseOffsetOut)
	{
		const uint8_t writeIdx = GetWriteIndex();

		uint32_t alignedSize = 0;
		switch (dataType)
		{
		case re::Buffer::DataType::Constant:
		{
			bufferNameOut = m_singleFrameUBOs[writeIdx];

			const GLint uboAlignment = opengl::SysInfo::GetUniformBufferOffsetAlignment(); // e.g. 256
			SEAssert(re::BufferAllocator::k_fixedAllocationByteSize % uboAlignment == 0,
				"Incompatible alignment");

			alignedSize = util::RoundUpToNearestMultiple<uint32_t>(size, uboAlignment);
		}
		break;
		case re::Buffer::DataType::Structured:
		{
			bufferNameOut = m_singleFrameSSBOs[writeIdx];

			const GLint ssboAlignment = opengl::SysInfo::GetShaderStorageBufferOffsetAlignment(); // e.g. 16
			SEAssert(re::BufferAllocator::k_fixedAllocationByteSize % ssboAlignment == 0,
				"Incompatible alignment");

			alignedSize = util::RoundUpToNearestMultiple<uint32_t>(size, ssboAlignment);
		}
		break;
		default: SEAssertF("Invalid DataType");
		}

		baseOffsetOut = AdvanceBaseIdx(dataType, alignedSize);
	}


	void BufferAllocator::Initialize(uint64_t currentFrame)
	{
		re::BufferAllocator::Initialize(currentFrame);

		// Note: OpenGL only supports double-buffering via a front and back buffer. Thus we can fill one buffer while
		// the other is in use, so long as we clear the buffer we're writing to at the beginning of each new frame
		
		// Generate our buffer names
		m_singleFrameUBOs.resize(m_numFramesInFlight, 0);
		glGenBuffers(m_numFramesInFlight, m_singleFrameUBOs.data());
		
		m_singleFrameSSBOs.resize(m_numFramesInFlight, 0);
		glGenBuffers(m_numFramesInFlight, m_singleFrameSSBOs.data());
		
		for (uint8_t bufferIdx = 0; bufferIdx < m_numFramesInFlight; bufferIdx++)
		{
			// UBO:
			// Binding associates the buffer object with the buffer object name
			glBindBuffer(GL_UNIFORM_BUFFER, m_singleFrameUBOs[bufferIdx]);

			SEAssert(glIsBuffer(m_singleFrameUBOs[bufferIdx]), "Buffer name is not valid");

			glBufferData(
				GL_UNIFORM_BUFFER,
				static_cast<GLsizeiptr>(re::BufferAllocator::k_fixedAllocationByteSize),
				nullptr, // NULL: Data store of the specified size is created, but remains uninitialized and thus undefined
				GL_DYNAMIC_DRAW);

			// RenderDoc label:
			glObjectLabel(
				GL_BUFFER, 
				m_singleFrameUBOs[bufferIdx], 
				-1, 
				std::format("Single-frame shared UBO {}", bufferIdx).c_str());

			// SSBO:
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_singleFrameSSBOs[bufferIdx]);

			SEAssert(glIsBuffer(m_singleFrameSSBOs[bufferIdx]), "Buffer name is not valid");

			glBufferData(
				GL_SHADER_STORAGE_BUFFER,
				static_cast<GLsizeiptr>(re::BufferAllocator::k_fixedAllocationByteSize),
				nullptr,
				GL_DYNAMIC_DRAW);

			glObjectLabel(
				GL_BUFFER,
				m_singleFrameSSBOs[bufferIdx],
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
			opengl::Buffer::Update(*entry.m_buffer, 0, 0, 0);
		}
	}


	void BufferAllocator::Destroy()
	{
		SEAssert(m_singleFrameUBOs.size() == m_singleFrameSSBOs.size() &&
			m_numFramesInFlight == m_singleFrameUBOs.size() &&
			m_numFramesInFlight == opengl::RenderManager::GetNumFramesInFlight(),
			"Mismatched number of single frame buffers");
		
		glDeleteBuffers(m_numFramesInFlight, m_singleFrameUBOs.data());
		glDeleteBuffers(m_numFramesInFlight, m_singleFrameSSBOs.data());

		m_singleFrameUBOs.assign(m_numFramesInFlight, 0);
		m_singleFrameSSBOs.assign(m_numFramesInFlight, 0);

		re::BufferAllocator::Destroy();
	}
}