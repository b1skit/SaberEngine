// © 2023 Adam Badke. All rights reserved.
#pragma once
#include <GL/glew.h>

#include "ParameterBlockAllocator.h"


namespace opengl
{
	class ParameterBlockAllocator
	{
	public:
		struct PlatformParams final : public re::ParameterBlockAllocator::PlatformParams
		{
			std::vector<GLuint> m_singleFrameUBOs;
			std::vector<GLuint> m_singleFrameSSBOs;
		};

		static void GetSubAllocation(
			re::ParameterBlock::PBDataType, uint32_t size, GLuint& bufferNameOut, GLintptr& baseOffsetOut);


	public:
		static void Create(re::ParameterBlockAllocator&);
		static void Destroy(re::ParameterBlockAllocator&);
	};
}