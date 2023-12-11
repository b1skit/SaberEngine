// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"


namespace gr
{
	class RenderCommandManager;


	class RenderCommandBuffer
	{
	public:
		RenderCommandBuffer(size_t allocationByteSize);
		~RenderCommandBuffer();


	protected:
		friend class RenderCommandManager;

		template<typename T, typename... Args>
		void Enqueue(Args&&... args);

		void Execute() const;

		void Reset();


	private:
		struct CommandMetadata
		{
			void* m_commandData;
			void(*Execute)(void*) = nullptr;
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
		RenderCommandBuffer() = delete;
		RenderCommandBuffer(RenderCommandBuffer const& rhs) = delete;
		RenderCommandBuffer(RenderCommandBuffer&& rhs) noexcept = delete;
		RenderCommandBuffer& operator=(RenderCommandBuffer const& rhs) = delete;
		RenderCommandBuffer& operator=(RenderCommandBuffer&& rhs) = delete;
	};


	template<typename T, typename... Args>
	void RenderCommandBuffer::Enqueue(Args&&... args)
	{
		SEAssert("No buffer allocation exists", m_buffer != nullptr);

		{
			std::unique_lock<std::mutex> lock(m_commandMetadataMutex);

			const size_t byteOffset = sizeof(PackedCommand<T>);

			// Reinterpret the required memory in our buffer as a PackedCommand:
			PackedCommand<T>* packedCommand = 
				reinterpret_cast<PackedCommand<T>*>(static_cast<uint8_t*>(m_buffer) + m_baseIdx);
			m_baseIdx += byteOffset;

			SEAssert("Render commands have overflowed. Consider increasing the allocation size", 
				m_baseIdx < m_bufferNumBytes);

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


	/******************************************************************************************************************/


	class RenderCommandManager
	{
	public:
		RenderCommandManager();

		template<typename T, typename... Args>
		void Enqueue(Args&&... args);

		void SwapBuffers();

		void Execute(); // Single-threaded execution to ensure deterministic command ordering


	private:
		uint8_t GetReadIdx() const;
		uint8_t GetWriteIdx() const;

		uint8_t m_writeIdx;
		uint8_t m_readIdx;


	private:
		static constexpr uint8_t k_numBuffers = 2; // Double-buffer our RenderCommandBuffers
		static constexpr size_t k_bufferAllocationSize = 16 * 1024 * 1024;

		std::array<std::unique_ptr<RenderCommandBuffer>, k_numBuffers> m_renderCommandBuffers;
		mutable std::shared_mutex m_renderCommandBuffersMutex;
	};


	template<typename T, typename... Args>
	inline void RenderCommandManager::Enqueue(Args&&... args)
	{
		m_renderCommandBuffers[GetWriteIdx()]->Enqueue<T>(std::forward<Args>(args)...);
	}
}