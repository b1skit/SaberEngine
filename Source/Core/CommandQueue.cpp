// © 2023 Adam Badke. All rights reserved.
#include "CommandQueue.h"


namespace core
{
	CommandBuffer::CommandBuffer(size_t allocationByteSize)
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


	CommandBuffer::~CommandBuffer()
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


	void CommandBuffer::Execute() const
	{
		// To ensure deterministic execution order, we execute commands single threaded via the CommandManager. We lock
		// our own mutex just to be safe, but it shouldn't be necessary as we're executing the CommandBuffer at the
		// reading index
		{
			std::unique_lock<std::mutex> lock(m_commandMetadataMutex);

			for (size_t cmdIdx = 0; cmdIdx < m_commandMetadata.size(); cmdIdx++)
			{
				m_commandMetadata[cmdIdx]->Execute(m_commandMetadata[cmdIdx]->m_commandData);
			}			
		}
	}


	void CommandBuffer::Reset()
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


	CommandManager::CommandManager(size_t bufferAllocationSize)
		: m_writeIdx(0)
		, m_readIdx(static_cast<uint8_t>(-1)) // Read index starts OOB
	{
		{
			std::unique_lock<std::mutex> lock(m_commandBuffersMutex);
			
			for (uint8_t bufferIdx = 0; bufferIdx < k_numBuffers; bufferIdx++)
			{
				m_commandBuffers[bufferIdx] = std::make_unique<CommandBuffer>(bufferAllocationSize);
			}
		}
	}


	void CommandManager::SwapBuffers()
	{
		{
			std::unique_lock<std::mutex> writeLock(m_commandBuffersMutex);

			m_readIdx = m_writeIdx;
			m_writeIdx = (m_writeIdx + 1) % k_numBuffers;
		}

		// No need to keep the mutex locked now that we've swapped the read/write indexes
		m_commandBuffers[m_writeIdx]->Reset();
	}


	void CommandManager::Execute()
	{
		// To ensure deterministic execution order, we execute commands single threaded
		m_commandBuffers[m_readIdx]->Execute();
	}


	uint8_t CommandManager::GetReadIdx() const
	{
		return m_readIdx;
	}


	uint8_t CommandManager::GetWriteIdx() const
	{
		return m_writeIdx;
	}


	/******************************************************************************************************************/


	FrameIndexedCommandManager::FrameIndexedCommandManager(size_t bufferAllocationSize, uint8_t numFramesInFlight)
		: m_lastEnqueuedFrameNum(k_invalidFrameNum)
		, m_lastExecutedFrameNum(k_invalidFrameNum)
		, m_numBuffers(numFramesInFlight)
	{
		{
			std::unique_lock<std::mutex> lock(m_commandBuffersMutex);

			for (uint8_t bufferIdx = 0; bufferIdx < m_numBuffers; bufferIdx++)
			{
				m_commandBuffers.emplace_back(std::make_unique<CommandBuffer>(bufferAllocationSize));
			}
		}
	}


	void FrameIndexedCommandManager::Execute(uint64_t frameNum)
	{
		SEAssert(frameNum > m_lastExecutedFrameNum || m_lastExecutedFrameNum == k_invalidFrameNum,
			"Trying to execute a frame that has already been executed");

		// To ensure deterministic execution order, we execute commands single threaded
		const uint8_t readIdx = GetReadIdx(frameNum);
		m_commandBuffers[readIdx]->Execute();
		m_commandBuffers[readIdx]->Reset();

		m_lastExecutedFrameNum = frameNum;
	}
}