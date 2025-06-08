// � 2023 Adam Badke. All rights reserved.
#pragma once
#include "BufferAllocator.h"


namespace opengl
{
	class BufferAllocator final : public virtual re::BufferAllocator
	{
	public:
		static uint32_t GetAlignedSize(uint32_t bufferByteSize, re::Buffer::UsageMask);


	public:
		BufferAllocator() = default;
		~BufferAllocator() override = default;

		void Initialize(uint64_t currentFrame) override;

		void Destroy() override;

		void BufferDefaultHeapDataPlatform(std::vector<PlatformCommitMetadata> const&, uint8_t frameOffsetIdx) override;


	public: // OpenGL-specific functionality:
		void GetSubAllocation(re::Buffer::UsageMask, uint32_t size, GLuint& bufferNameOut, GLintptr& baseOffsetOut);


	private:
		std::array<std::vector<GLuint>, re::BufferAllocator::AllocationPool_Count> m_singleFrameBuffers;
	};
}