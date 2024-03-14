// © 2023 Adam Badke. All rights reserved.
#pragma once
#include <GL/glew.h>

#include "BufferAllocator.h"


namespace opengl
{
	class BufferAllocator
	{
	public:
		struct PlatformParams final : public re::BufferAllocator::PlatformParams
		{
			std::vector<GLuint> m_singleFrameUBOs;
			std::vector<GLuint> m_singleFrameSSBOs;
		};

		static void GetSubAllocation(
			re::Buffer::DataType, uint32_t size, GLuint& bufferNameOut, GLintptr& baseOffsetOut);


	public:
		static void Create(re::BufferAllocator&);
		static void Destroy(re::BufferAllocator&);
	};
}