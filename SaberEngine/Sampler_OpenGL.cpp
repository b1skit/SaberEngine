#pragma once

#include <assert.h>

#include "DebugConfiguration.h"
#include "Sampler_OpenGL.h"
#include "Sampler_Platform.h"


namespace opengl
{
	Sampler::PlatformParams::PlatformParams(gr::Sampler::SamplerParams const& samplerParams)
	{
		// Minification filter:
		/*********************/
		switch (samplerParams.m_texMinMode)
		{
		case gr::Sampler::TextureMinFilter::Nearest:
		{
			m_textureMinFilter = GL_NEAREST;
		}
		break;
		case gr::Sampler::TextureMinFilter::NearestMipMapLinear:
		{
			m_textureMinFilter = GL_NEAREST_MIPMAP_LINEAR;
		}
		break;
		case gr::Sampler::TextureMinFilter::Linear:
		{
			m_textureMinFilter = GL_LINEAR;
		}
		break;
		case gr::Sampler::TextureMinFilter::LinearMipMapLinear:
		{
			m_textureMinFilter = GL_LINEAR_MIPMAP_LINEAR;
		}
		break;
		default:
			SEAssertF("Invalid/unsupported texture min mode");
		}

		// Magnification filter:
		/*********************/
		switch (samplerParams.m_texMaxMode)
		{
		case gr::Sampler::TextureMaxFilter::Nearest: // Point sampling
		{
			m_textureMaxFilter = GL_NEAREST;
		}
		break;
		case gr::Sampler::TextureMaxFilter::Linear: // Weighted linear blend
		{
			m_textureMaxFilter = GL_LINEAR;
		}

		break;
		default:
			SEAssertF("Invalid/unsupported texture max mode");
		}

		// Sampler mode:
		/***************/
		switch (samplerParams.m_texSamplerMode)
		{
		case gr::Sampler::TextureSamplerMode::Wrap:
		{
			m_textureWrapS = GL_REPEAT;
			m_textureWrapT = GL_REPEAT;
			m_textureWrapR = GL_REPEAT;
		}
		break;
		case gr::Sampler::TextureSamplerMode::Mirrored:
		{
			m_textureWrapS = GL_MIRRORED_REPEAT;
			m_textureWrapT = GL_MIRRORED_REPEAT;
			m_textureWrapR = GL_MIRRORED_REPEAT;
		}
		break;
		case gr::Sampler::TextureSamplerMode::Clamp:
		{
			m_textureWrapS = GL_CLAMP_TO_EDGE;
			m_textureWrapT = GL_CLAMP_TO_EDGE;
			m_textureWrapR = GL_CLAMP_TO_EDGE;
		}
		break;
		default:
			SEAssertF("Invalid/unsupported texture max mode");
		}
	}


	Sampler::PlatformParams::~PlatformParams()
	{
		glDeleteSamplers(1, &m_samplerID);
	}


	void Sampler::Create(gr::Sampler& sampler)
	{
		LOG("Creating sampler: \"%s\"", sampler.GetName().c_str());

		PlatformParams* const params =
			dynamic_cast<opengl::Sampler::PlatformParams* const>(sampler.GetPlatformParams());

		SEAssert("Attempting to create a sampler that already has been created", !glIsSampler(params->m_samplerID));

		glGenSamplers(1, &params->m_samplerID);
		glBindSampler(0, params->m_samplerID);

		// RenderDoc object name:
		glObjectLabel(GL_SAMPLER, params->m_samplerID, -1, (sampler.GetName() + " sampler").c_str());

		if (!glIsSampler(params->m_samplerID))
		{
			LOG_ERROR("Texture sampler creation failed");
			SEAssert("Texture sampler creation failed", glIsSampler(params->m_samplerID));
		}

		glSamplerParameteri(params->m_samplerID, GL_TEXTURE_WRAP_S, params->m_textureWrapS);
		glSamplerParameteri(params->m_samplerID, GL_TEXTURE_WRAP_T, params->m_textureWrapT);
		glSamplerParameteri(params->m_samplerID, GL_TEXTURE_WRAP_R, params->m_textureWrapR);

		glSamplerParameteri(params->m_samplerID, GL_TEXTURE_MIN_FILTER, params->m_textureMinFilter);
		glSamplerParameteri(params->m_samplerID, GL_TEXTURE_MAG_FILTER, params->m_textureMaxFilter);

		// Note: We leave the sampler bound
	}


	void Sampler::Bind(gr::Sampler const& sampler, uint32_t textureUnit, bool doBind)
	{
		PlatformParams const* const params =
			dynamic_cast<opengl::Sampler::PlatformParams const* const>(sampler.GetPlatformParams());

		if (doBind)
		{
			glBindSampler(textureUnit, params->m_samplerID);
		}
		else
		{
			glBindSampler(textureUnit, 0);
		}
	}


	void Sampler::Destroy(gr::Sampler& sampler)
	{
		PlatformParams* const params =
			dynamic_cast<opengl::Sampler::PlatformParams* const>(sampler.GetPlatformParams());

		glDeleteSamplers(1, &params->m_samplerID);
		params->m_samplerID = 0;
	}
}