#pragma once

#include <GL/glew.h>

#include "ParameterBlock_Platform.h"


namespace opengl
{
	class ParameterBlock
	{
	public:
		struct PlatformParams : public virtual platform::ParameterBlock::PlatformParams
		{
			PlatformParams() = default;
			~PlatformParams() override = default;

			// OpenGL-specific parameters:
			GLuint m_ssbo = 0; // Shader Storage Buffer Object (SSBO) handle
		};
	public:
		// ParameterBlock platform handles:
		static void Create(re::ParameterBlock& paramBlock);
		static void Update(re::ParameterBlock& paramBlock);
		static void Destroy(re::ParameterBlock& paramBlock);

		// OpenGL-specific functionality:
		static void Bind(re::ParameterBlock const& paramBlock, GLuint uniformBlockIdx);

	private:

	};
}