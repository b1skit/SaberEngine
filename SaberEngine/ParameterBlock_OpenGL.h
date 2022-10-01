#pragma once

#include <GL/glew.h>

#include "ParameterBlock_Platform.h"


namespace opengl
{
	class PermanentParameterBlock
	{
	public:
		struct PlatformParams : public virtual platform::PermanentParameterBlock::PlatformParams
		{
			PlatformParams() = default;
			~PlatformParams() override = default;

			// OpenGL-specific parameters:
			GLuint m_ssbo = 0; // Shader Storage Buffer Object (SSBO) handle
		};
	public:
		// PermanentParameterBlock platform handles:
		static void Create(re::PermanentParameterBlock& paramBlock);
		static void Destroy(re::PermanentParameterBlock& paramBlock);

		// OpenGL-specific functionality:
		static void Bind(re::PermanentParameterBlock const& paramBlock, GLuint uniformBlockIdx);

	private:

	};
}