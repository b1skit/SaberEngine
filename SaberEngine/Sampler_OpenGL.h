#pragma once

#include <GL/glew.h>

#include "Sampler_Platform.h"
#include "Sampler.h"


namespace opengl
{
	class Sampler
	{
	public:
		struct PlatformParams final : public virtual gr::Sampler::PlatformParams
		{
			PlatformParams(gr::Sampler::SamplerParams const& samplerParams);
			~PlatformParams() override;

			GLuint m_samplerID;		// Name of a sampler

			GLenum m_textureWrapS;
			GLenum m_textureWrapT;
			GLenum m_textureWrapR;

			GLenum m_textureMinFilter;
			GLenum m_textureMaxFilter;	

			// Samplers are uniquely bound to the GPU, thus they should not be duplicated
			PlatformParams() = delete;
			PlatformParams(PlatformParams const&) = delete;
			PlatformParams(PlatformParams&&) = delete;
			PlatformParams& operator=(PlatformParams const&) = delete;
		};

	public:
		static void Create(gr::Sampler& sampler);
		static void Bind(gr::Sampler& sampler, uint32_t textureUnit, bool doBind);
		static void Destroy(gr::Sampler& sampler);

	private:

	};
}