// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Core\Assert.h"


namespace en
{
	class CommandManager;


	class CommandBuffer
	{
	public:
		CommandBuffer(size_t allocationByteSize);
		~CommandBuffer();


	protected:
		friend class CommandManager;
		friend class FrameIndexedCommandManager;

		template<typename T, typename... Args>
		void Enqueue(Args&&... args);

		void Execute() const;
		
		bool HasCommandsToExecute() const;

		void Reset();


	private:
		struct CommandMetadata
		{
			void* m_commandData;
			void (*Execute)(void*) = nullptr;
			void (*Destroy)(void*) = nullptr;
		};

		template<typename T>
		struct PackedCommand
		{
			CommandMetadata m_metadata;
			T m_commandData;
		};

	private:
		void* m_buffer;
		size_t m_baseIdx;
		const size_t m_bufferNumBytes;

		std::vector<CommandMetadata*> m_commandMetadata;
		mutable std::mutex m_commandMetadataMutex;


	private: 
		CommandBuffer() = delete;
		CommandBuffer(CommandBuffer const& rhs) = delete;
		CommandBuffer(CommandBuffer&& rhs) noexcept = delete;
		CommandBuffer& operator=(CommandBuffer const& rhs) = delete;
		CommandBuffer& operator=(CommandBuffer&& rhs) = delete;
	};


	template<typename T, typename... Args>
	void CommandBuffer::Enqueue(Args&&... args)
	{
		SEAssert(m_buffer != nullptr, "No buffer allocation exists");

		{
			std::unique_lock<std::mutex> lock(m_commandMetadataMutex);

			const size_t byteOffset = sizeof(PackedCommand<T>);

			// Reinterpret the required memory in our buffer as a PackedCommand:
			PackedCommand<T>* packedCommand = 
				reinterpret_cast<PackedCommand<T>*>(static_cast<uint8_t*>(m_buffer) + m_baseIdx);
			m_baseIdx += byteOffset;

			SEAssert(m_baseIdx < m_bufferNumBytes,
				"Commands have overflowed. Consider increasing the allocation size");

			// Place our data:
			new (&packedCommand->m_metadata) CommandMetadata();
			T* newCommand = new (&packedCommand->m_commandData) T(std::forward<Args>(args)...);

			// Set the metadata so we can access everything later on:
			packedCommand->m_metadata.m_commandData = newCommand;
			packedCommand->m_metadata.Execute = &newCommand->T::Execute;
			packedCommand->m_metadata.Destroy = &newCommand->T::Destroy;
			
			m_commandMetadata.emplace_back(&packedCommand->m_metadata);
		}
	}


	inline bool CommandBuffer::HasCommandsToExecute() const
	{
		{
			std::unique_lock<std::mutex> lock(m_commandMetadataMutex);
			return !m_commandMetadata.empty();
		}
	}


	/******************************************************************************************************************/


	class CommandManager
	{
	public:
		CommandManager(size_t bufferAllocationSize);

		template<typename T, typename... Args>
		void Enqueue(Args&&... args);

		void SwapBuffers();

		void Execute(); // Single-threaded execution to ensure deterministic command ordering

		bool HasCommandsToExecute() const;


	private:
		uint8_t GetReadIdx() const;
		uint8_t GetWriteIdx() const;

		uint8_t m_writeIdx;
		uint8_t m_readIdx;


	private:
		static constexpr uint8_t k_numBuffers = 2; // Double-buffer our CommandBuffers

		std::array<std::unique_ptr<CommandBuffer>, k_numBuffers> m_commandBuffers;
		mutable std::mutex m_commandBuffersMutex;
	};


	template<typename T, typename... Args>
	inline void CommandManager::Enqueue(Args&&... args)
	{
		m_commandBuffers[GetWriteIdx()]->Enqueue<T>(std::forward<Args>(args)...);
	}


	inline bool CommandManager::HasCommandsToExecute() const
	{
		return m_commandBuffers[GetReadIdx()]->HasCommandsToExecute();
	}


	/******************************************************************************************************************/


	class FrameIndexedCommandManager
	{
	public:
		FrameIndexedCommandManager(size_t bufferAllocationSize);

		template<typename T, typename... Args>
		void Enqueue(uint64_t frameNum, Args&&... args);

		void Execute(uint64_t frameNum); // Single-threaded execution to ensure deterministic command ordering

		bool HasCommandsToExecute(uint64_t frameNum) const;


	private:
		uint8_t GetReadIdx(uint64_t frameNum) const;
		uint8_t GetWriteIdx(uint64_t frameNum) const;


	private:
		static constexpr uint64_t k_invalidFrameNum = std::numeric_limits<uint64_t>::max();
		uint64_t m_lastEnqueuedFrameNum;
		uint64_t m_lastExecutedFrameNum;

		uint8_t m_numBuffers; // Number of frames in flight
		
		std::vector<std::unique_ptr<CommandBuffer>> m_commandBuffers;
		mutable std::mutex m_commandBuffersMutex;
	};


	template<typename T, typename... Args>
	inline void FrameIndexedCommandManager::Enqueue(uint64_t frameNum, Args&&... args)
	{
		SEAssert(frameNum > m_lastExecutedFrameNum || m_lastExecutedFrameNum == k_invalidFrameNum,
			"Trying to enqueue for a frame that has already been executed");

		SEAssert(frameNum >= m_lastEnqueuedFrameNum || m_lastEnqueuedFrameNum == k_invalidFrameNum,
			"Trying to enqueue for a non-monotonically-increasing frame number");

		SEAssert(frameNum == m_lastEnqueuedFrameNum || !m_commandBuffers[GetWriteIdx(frameNum)]->HasCommandsToExecute(),
			"Trying to enqueue work for a new frame, but the buffer still contains old elements");

		m_commandBuffers[GetWriteIdx(frameNum)]->Enqueue<T>(std::forward<Args>(args)...);

		m_lastEnqueuedFrameNum = frameNum;
	}


	inline bool FrameIndexedCommandManager::HasCommandsToExecute(uint64_t frameNum) const
	{
		return m_commandBuffers[GetReadIdx(frameNum)]->HasCommandsToExecute();
	}


	inline uint8_t FrameIndexedCommandManager::GetReadIdx(uint64_t frameNum) const
	{
		return frameNum % m_numBuffers;
	}


	inline uint8_t FrameIndexedCommandManager::GetWriteIdx(uint64_t frameNum) const
	{
		return frameNum % m_numBuffers;
	}
}