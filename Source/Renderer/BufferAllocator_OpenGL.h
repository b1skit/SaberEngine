// © 2023 Adam Badke. All rights reserved.
#pragma once
#include <GL/glew.h>

#include "BufferAllocator.h"


namespace opengl
{
	class BufferAllocator final : public virtual re::BufferAllocator
	{
	public:
		BufferAllocator() = default;
		~BufferAllocator() override = default;

		void Initialize(uint64_t currentFrame) override;

		void Destroy() override;

		void BufferDataPlatform() override;


	public: // OpenGL-specific functionality:
		void GetSubAllocation(re::Buffer::Type, uint32_t size, GLuint& bufferNameOut, GLintptr& baseOffsetOut);


	private:
		std::vector<GLuint> m_singleFrameUBOs;
		std::vector<GLuint> m_singleFrameSSBOs;
	};
}