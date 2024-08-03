// © 2022 Adam Badke. All rights reserved.
#pragma once

#include <GL/glew.h>

#include "Sampler_Platform.h"
#include "Sampler.h"


namespace opengl
{
	class Sampler
	{
	public:
		struct PlatformParams final : public re::Sampler::PlatformParams
		{
			GLuint m_samplerID;		// Name of a sampler

			GLenum m_textureWrapS;
			GLenum m_textureWrapT;
			GLenum m_textureWrapR;

			GLenum m_textureMinFilter;
			GLenum m_textureMagFilter;

			GLenum m_comparisonFunc;
		};

	public:
		static void Create(re::Sampler& sampler);
		static void Destroy(re::Sampler& sampler);

		// OpenGL-specific functionality:
		static void Bind(re::Sampler const& sampler, uint32_t textureUnit);
	};
}