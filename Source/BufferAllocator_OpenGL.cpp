// © 2023 Adam Badke. All rights reserved.
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
		re::BufferAllocator& ba = re::Context::Get()->GetBufferAllocator();
		opengl::BufferAllocator::PlatformParams* baPlatParams =
			ba.GetPlatformParams()->As<opengl::BufferAllocator::PlatformParams*>();

		const uint8_t writeIdx = baPlatParams->GetWriteIndex();

		uint32_t alignedSize = 0;
		switch (dataType)
		{
		case re::Buffer::DataType::Constant:
		{
			bufferNameOut = baPlatParams->m_singleFrameUBOs[writeIdx];

			const GLint uboAlignment = opengl::SysInfo::GetUniformBufferOffsetAlignment(); // e.g. 256
			SEAssert(re::BufferAllocator::k_fixedAllocationByteSize % uboAlignment == 0,
				"Incompatible alignment");

			alignedSize = util::RoundUpToNearestMultiple<uint32_t>(size, uboAlignment);
		}
		break;
		case re::Buffer::DataType::Structured:
		{
			bufferNameOut = baPlatParams->m_singleFrameSSBOs[writeIdx];

			const GLint ssboAlignment = opengl::SysInfo::GetShaderStorageBufferOffsetAlignment(); // e.g. 16
			SEAssert(re::BufferAllocator::k_fixedAllocationByteSize % ssboAlignment == 0,
				"Incompatible alignment");

			alignedSize = util::RoundUpToNearestMultiple<uint32_t>(size, ssboAlignment);
		}
		break;
		default: SEAssertF("Invalid DataType");
		}

		baseOffsetOut = baPlatParams->AdvanceBaseIdx(dataType, alignedSize);
	}


	void BufferAllocator::Create(re::BufferAllocator& ba)
	{
		// Note: OpenGL only supports double-buffering via a front and back buffer. Thus we can fill one buffer while
		// the other is in use, so long as we clear the buffer we're writing to at the beginning of each new frame

		opengl::BufferAllocator::PlatformParams* baPlatformParams = 
			ba.GetPlatformParams()->As<opengl::BufferAllocator::PlatformParams*>();	
		
		// Generate our buffer names
		baPlatformParams->m_singleFrameUBOs.resize(baPlatformParams->m_numBuffers, 0);
		glGenBuffers(baPlatformParams->m_numBuffers, baPlatformParams->m_singleFrameUBOs.data());
		
		baPlatformParams->m_singleFrameSSBOs.resize(baPlatformParams->m_numBuffers, 0);
		glGenBuffers(baPlatformParams->m_numBuffers, baPlatformParams->m_singleFrameSSBOs.data());
		
		for (uint8_t bufferIdx = 0; bufferIdx < baPlatformParams->m_numBuffers; bufferIdx++)
		{
			// UBO:
			// Binding associates the buffer object with the buffer object name
			glBindBuffer(GL_UNIFORM_BUFFER, baPlatformParams->m_singleFrameUBOs[bufferIdx]);

			SEAssert(glIsBuffer(baPlatformParams->m_singleFrameUBOs[bufferIdx]), "Buffer name is not valid");

			glBufferData(
				GL_UNIFORM_BUFFER,
				static_cast<GLsizeiptr>(re::BufferAllocator::k_fixedAllocationByteSize),
				nullptr, // NULL: Data store of the specified size is created, but remains uninitialized and thus undefined
				GL_DYNAMIC_DRAW);

			// RenderDoc label:
			glObjectLabel(
				GL_BUFFER, 
				baPlatformParams->m_singleFrameUBOs[bufferIdx], 
				-1, 
				std::format("Single-frame shared UBO {}", bufferIdx).c_str());

			// SSBO:
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, baPlatformParams->m_singleFrameSSBOs[bufferIdx]);

			SEAssert(glIsBuffer(baPlatformParams->m_singleFrameSSBOs[bufferIdx]), "Buffer name is not valid");

			glBufferData(
				GL_SHADER_STORAGE_BUFFER,
				static_cast<GLsizeiptr>(re::BufferAllocator::k_fixedAllocationByteSize),
				nullptr,
				GL_DYNAMIC_DRAW);

			glObjectLabel(
				GL_BUFFER,
				baPlatformParams->m_singleFrameSSBOs[bufferIdx],
				-1,
				std::format("Single-frame shared SSBO {}", bufferIdx).c_str());
		}
	}


	void BufferAllocator::Destroy(re::BufferAllocator& ba)
	{
		opengl::BufferAllocator::PlatformParams* baPlatformParams =
			ba.GetPlatformParams()->As<opengl::BufferAllocator::PlatformParams*>();

		SEAssert(baPlatformParams->m_singleFrameUBOs.size() == baPlatformParams->m_singleFrameSSBOs.size() &&
			baPlatformParams->m_numBuffers == baPlatformParams->m_singleFrameUBOs.size() &&
			baPlatformParams->m_numBuffers == opengl::RenderManager::GetNumFramesInFlight(),
			"Mismatched number of single frame buffers");
		
		glDeleteBuffers(baPlatformParams->m_numBuffers, baPlatformParams->m_singleFrameUBOs.data());
		glDeleteBuffers(baPlatformParams->m_numBuffers, baPlatformParams->m_singleFrameSSBOs.data());

		baPlatformParams->m_singleFrameUBOs.assign(baPlatformParams->m_numBuffers, 0);
		baPlatformParams->m_singleFrameSSBOs.assign(baPlatformParams->m_numBuffers, 0);
	}
}