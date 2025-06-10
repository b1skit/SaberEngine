// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Sampler_Platform.h"
#include "Sampler.h"


namespace opengl
{
	class Sampler
	{
	public:
		struct PlatObj final : public re::Sampler::PlatObj
		{
			GLuint m_samplerID = 0;		// Name of a sampler

			GLenum m_textureWrapS = 0;
			GLenum m_textureWrapT = 0;
			GLenum m_textureWrapR = 0;

			GLenum m_textureMinFilter = 0;
			GLenum m_textureMagFilter = 0;

			GLenum m_comparisonFunc = 0;
		};

	public:
		static void Create(re::Sampler& sampler);
		static void Destroy(re::Sampler& sampler);

		// OpenGL-specific functionality:
		static void Bind(re::Sampler const& sampler, uint32_t textureUnit);
	};
}