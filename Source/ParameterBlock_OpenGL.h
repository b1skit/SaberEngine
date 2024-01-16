// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <GL/glew.h>

#include "ParameterBlock_Platform.h"
#include "ParameterBlock.h"


namespace opengl
{
	class ParameterBlock
	{
	public:
		struct PlatformParams final : public re::ParameterBlock::PlatformParams
		{
			GLuint m_bufferName; // UBO or SSBO handle
			GLintptr m_baseOffset; // 0 for permanent PBs, or >= 0 for single-frame PBs
		};


	public:
		static void Create(re::ParameterBlock& paramBlock);
		static void Update(re::ParameterBlock const& paramBlock, uint8_t heapOffsetFactor);
		static void Destroy(re::ParameterBlock& paramBlock);


	public: // OpenGL-specific functionality:		
		static void Bind(re::ParameterBlock const& paramBlock, GLuint uniformBlockIdx);
	};
}