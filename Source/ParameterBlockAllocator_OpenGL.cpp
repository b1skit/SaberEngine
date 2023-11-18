// © 2023 Adam Badke. All rights reserved.
#include "MathUtils.h"
#include "ParameterBlockAllocator.h"
#include "ParameterBlockAllocator_OpenGL.h"
#include "RenderManager_OpenGL.h"
#include "SysInfo_OpenGL.h"


namespace opengl
{
	void ParameterBlockAllocator::GetSubAllocation(
		re::ParameterBlock::PBDataType pbDataType, 
		uint32_t size, 
		GLuint& bufferNameOut,
		GLintptr& baseOffsetOut)
	{
		re::ParameterBlockAllocator& pba = re::Context::Get()->GetParameterBlockAllocator();
		opengl::ParameterBlockAllocator::PlatformParams* pbaPlatParams =
			pba.GetPlatformParams()->As<opengl::ParameterBlockAllocator::PlatformParams*>();

		const uint8_t writeIdx = pbaPlatParams->GetWriteIndex();

		uint32_t alignedSize = 0;
		switch (pbDataType)
		{
		case re::ParameterBlock::PBDataType::SingleElement:
		{
			bufferNameOut = pbaPlatParams->m_singleFrameUBOs[writeIdx];

			const GLint uboAlignment = opengl::SysInfo::GetUniformBufferOffsetAlignment(); // e.g. 256
			SEAssert("Incompatible alignment",
				re::ParameterBlockAllocator::k_fixedAllocationByteSize % uboAlignment == 0);

			alignedSize = util::RoundUpToNearestMultiple<uint32_t>(size, uboAlignment);
		}
		break;
		case re::ParameterBlock::PBDataType::Array:
		{
			bufferNameOut = pbaPlatParams->m_singleFrameSSBOs[writeIdx];

			const GLint ssboAlignment = opengl::SysInfo::GetShaderStorageBufferOffsetAlignment(); // e.g. 16
			SEAssert("Incompatible alignment", 
				re::ParameterBlockAllocator::k_fixedAllocationByteSize % ssboAlignment == 0);

			alignedSize = util::RoundUpToNearestMultiple<uint32_t>(size, ssboAlignment);
		}
		break;
		default: SEAssertF("Invalid PBDataType");
		}

		baseOffsetOut = pbaPlatParams->AdvanceBaseIdx(pbDataType, alignedSize);
	}


	void ParameterBlockAllocator::Create(re::ParameterBlockAllocator& pba)
	{
		// Note: OpenGL only supports double-buffering via a front and back buffer. Thus we can fill one buffer while
		// the other is in use, so long as we clear the buffer we're writing to at the beginning of each new frame

		opengl::ParameterBlockAllocator::PlatformParams* pbaPlatformParams = 
			pba.GetPlatformParams()->As<opengl::ParameterBlockAllocator::PlatformParams*>();	
		
		// Generate our buffer names
		pbaPlatformParams->m_singleFrameUBOs.resize(pbaPlatformParams->m_numBuffers, 0);
		glGenBuffers(pbaPlatformParams->m_numBuffers, pbaPlatformParams->m_singleFrameUBOs.data());
		
		pbaPlatformParams->m_singleFrameSSBOs.resize(pbaPlatformParams->m_numBuffers, 0);
		glGenBuffers(pbaPlatformParams->m_numBuffers, pbaPlatformParams->m_singleFrameSSBOs.data());
		
		for (uint8_t bufferIdx = 0; bufferIdx < pbaPlatformParams->m_numBuffers; bufferIdx++)
		{
			// UBO:
			// Binding associates the buffer object with the buffer object name
			glBindBuffer(GL_UNIFORM_BUFFER, pbaPlatformParams->m_singleFrameUBOs[bufferIdx]);

			SEAssert("Buffer name is not valid", glIsBuffer(pbaPlatformParams->m_singleFrameUBOs[bufferIdx]));

			glBufferData(
				GL_UNIFORM_BUFFER,
				static_cast<GLsizeiptr>(re::ParameterBlockAllocator::k_fixedAllocationByteSize),
				nullptr, // NULL: Data store of the specified size is created, but remains uninitialized and thus undefined
				GL_DYNAMIC_DRAW);

			// RenderDoc label:
			glObjectLabel(
				GL_BUFFER, 
				pbaPlatformParams->m_singleFrameUBOs[bufferIdx], 
				-1, 
				std::format("Single-frame shared UBO {}", bufferIdx).c_str());

			// SSBO:
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, pbaPlatformParams->m_singleFrameSSBOs[bufferIdx]);

			SEAssert("Buffer name is not valid", glIsBuffer(pbaPlatformParams->m_singleFrameSSBOs[bufferIdx]));

			glBufferData(
				GL_SHADER_STORAGE_BUFFER,
				static_cast<GLsizeiptr>(re::ParameterBlockAllocator::k_fixedAllocationByteSize),
				nullptr,
				GL_DYNAMIC_DRAW);

			glObjectLabel(
				GL_BUFFER,
				pbaPlatformParams->m_singleFrameSSBOs[bufferIdx],
				-1,
				std::format("Single-frame shared SSBO {}", bufferIdx).c_str());
		}
	}


	void ParameterBlockAllocator::Destroy(re::ParameterBlockAllocator& pba)
	{
		opengl::ParameterBlockAllocator::PlatformParams* pbaPlatformParams =
			pba.GetPlatformParams()->As<opengl::ParameterBlockAllocator::PlatformParams*>();

		SEAssert("Mismatched number of single frame buffers", 
			pbaPlatformParams->m_singleFrameUBOs.size() == pbaPlatformParams->m_singleFrameSSBOs.size() &&
			pbaPlatformParams->m_numBuffers == pbaPlatformParams->m_singleFrameUBOs.size() &&
			pbaPlatformParams->m_numBuffers == opengl::RenderManager::GetNumFrames());
		
		glDeleteBuffers(pbaPlatformParams->m_numBuffers, pbaPlatformParams->m_singleFrameUBOs.data());
		glDeleteBuffers(pbaPlatformParams->m_numBuffers, pbaPlatformParams->m_singleFrameSSBOs.data());

		pbaPlatformParams->m_singleFrameUBOs.assign(pbaPlatformParams->m_numBuffers, 0);
		pbaPlatformParams->m_singleFrameSSBOs.assign(pbaPlatformParams->m_numBuffers, 0);
	}
}