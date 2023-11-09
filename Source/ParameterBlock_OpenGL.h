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
		};


	public:
		// ParameterBlock platform handles:
		static void Create(re::ParameterBlock& paramBlock);
		static void Update(re::ParameterBlock& paramBlock);
		static void Destroy(re::ParameterBlock& paramBlock);

	public:
		// OpenGL-specific functionality:
		static void Bind(re::ParameterBlock const& paramBlock, GLuint uniformBlockIdx);
	};
}