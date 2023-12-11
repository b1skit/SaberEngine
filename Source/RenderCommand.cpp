// © 2023 Adam Badke. All rights reserved.
#include "RenderCommand.h"


namespace gr
{
	RenderCommandBuffer::RenderCommandBuffer(size_t allocationByteSize)
		: m_buffer(nullptr)
		, m_baseIdx(0)
		, m_bufferNumBytes(allocationByteSize)		
	{
		{
			std::unique_lock<std::mutex> lock(m_commandMetadataMutex);

			m_buffer = malloc(m_bufferNumBytes);

			// As a micro-optimization, reserve a reasonable amount of space in the metadata vector
			constexpr size_t expectedAllocationChunkByteSize = 64;
			m_commandMetadata.reserve(allocationByteSize / expectedAllocationChunkByteSize);
		}
	}


	RenderCommandBuffer::~RenderCommandBuffer()
	{
		Reset();
		{
			std::unique_lock<std::mutex> lock(m_commandMetadataMutex);
			if (m_buffer != nullptr)
			{
				delete m_buffer;
				m_buffer = nullptr;
				m_baseIdx = 0;
			}
		}
	}


	void RenderCommandBuffer::Execute() const
	{
		// To ensure deterministic execution order, we execute render commands single threaded via the 
		// RenderCommandManager. We lock our own mutex just to be safe, but it shouldn't be necessary as we're executing
		// the RenderCommandBuffer at the reading index
		{
			std::unique_lock<std::mutex> lock(m_commandMetadataMutex);

			for (size_t cmdIdx = 0; cmdIdx < m_commandMetadata.size(); cmdIdx++)
			{
				m_commandMetadata[cmdIdx]->Execute(m_commandMetadata[cmdIdx]->m_commandData);
			}			
		}
	}


	void RenderCommandBuffer::Reset()
	{
		{
			std::unique_lock<std::mutex> lock(m_commandMetadataMutex);

			// Even though we own the backing memory, we manually call the command dtors incase they're complex types
			for (size_t cmdIdx = 0; cmdIdx < m_commandMetadata.size(); cmdIdx++)
			{
				m_commandMetadata[cmdIdx]->Destroy(m_commandMetadata[cmdIdx]->m_commandData);
			}
			m_commandMetadata.clear();
			m_baseIdx = 0;
		}
	}


	/******************************************************************************************************************/


	RenderCommandManager::RenderCommandManager()
		: m_writeIdx(0)
		, m_readIdx(static_cast<uint8_t>(-1)) // Read index starts OOB
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_renderCommandBuffersMutex);
			
			for (uint8_t bufferIdx = 0; bufferIdx < k_numBuffers; bufferIdx++)
			{
				m_renderCommandBuffers[bufferIdx] = std::make_unique<RenderCommandBuffer>(k_bufferAllocationSize);
			}
		}
	}


	void RenderCommandManager::SwapBuffers()
	{
		{
			std::unique_lock<std::shared_mutex> writeLock(m_renderCommandBuffersMutex);

			m_readIdx = m_writeIdx;
			m_writeIdx = (m_writeIdx + 1) % k_numBuffers;
		}

		// No need to keep the mutex locked now that we've swapped the read/write indexes
		m_renderCommandBuffers[m_writeIdx]->Reset();
	}


	void RenderCommandManager::Execute()
	{
		// To ensure deterministic execution order, we execute render commands single threaded
		m_renderCommandBuffers[m_readIdx]->Execute();
	}


	uint8_t RenderCommandManager::GetReadIdx() const
	{
		return m_readIdx;
	}


	uint8_t RenderCommandManager::GetWriteIdx() const
	{
		return m_writeIdx;
	}
}