// � 2022 Adam Badke. All rights reserved.
#pragma once

#include "Assert.h"
#include "Sampler_OpenGL.h"
#include "Sampler_Platform.h"


namespace opengl
{
	Sampler::PlatformParams::PlatformParams(re::Sampler::SamplerParams const& samplerParams)
	{
		// Minification filter:
		/*********************/
		switch (samplerParams.m_texMinMode)
		{
		case re::Sampler::MinFilter::Nearest:
		{
			m_textureMinFilter = GL_NEAREST;
		}
		break;
		case re::Sampler::MinFilter::NearestMipMapLinear:
		{
			m_textureMinFilter = GL_NEAREST_MIPMAP_LINEAR;
		}
		break;
		case re::Sampler::MinFilter::Linear:
		{
			m_textureMinFilter = GL_LINEAR;
		}
		break;
		case re::Sampler::MinFilter::LinearMipMapLinear:
		{
			m_textureMinFilter = GL_LINEAR_MIPMAP_LINEAR;
		}
		break;
		default:
			SEAssertF("Invalid/unsupported texture min mode");
		}

		// Magnification filter:
		/*********************/
		switch (samplerParams.m_texMagMode)
		{
		case re::Sampler::MagFilter::Nearest: // Point sampling
		{
			m_textureMaxFilter = GL_NEAREST;
		}
		break;
		case re::Sampler::MagFilter::Linear: // Weighted linear blend
		{
			m_textureMaxFilter = GL_LINEAR;
		}

		break;
		default:
			SEAssertF("Invalid/unsupported texture max mode");
		}

		// Sampler mode:
		/***************/
		switch (samplerParams.m_addressMode)
		{
		case re::Sampler::AddressMode::Wrap:
		{
			m_textureWrapS = GL_REPEAT;
			m_textureWrapT = GL_REPEAT;
			m_textureWrapR = GL_REPEAT;
		}
		break;
		case re::Sampler::AddressMode::Mirror:
		{
			m_textureWrapS = GL_MIRRORED_REPEAT;
			m_textureWrapT = GL_MIRRORED_REPEAT;
			m_textureWrapR = GL_MIRRORED_REPEAT;
		}
		break;
		case re::Sampler::AddressMode::Clamp:
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


	void Sampler::Create(re::Sampler& sampler)
	{
		SEAssert(!sampler.GetPlatformParams()->m_isCreated, "Sampler is already created");

		LOG("Creating sampler: \"%s\"", sampler.GetName().c_str());

		opengl::Sampler::PlatformParams* params = sampler.GetPlatformParams()->As<opengl::Sampler::PlatformParams*>();

		SEAssert(!glIsSampler(params->m_samplerID), "Attempting to create a sampler that already has been created");

		glGenSamplers(1, &params->m_samplerID);
		glBindSampler(0, params->m_samplerID);

		// RenderDoc object name:
		glObjectLabel(GL_SAMPLER, params->m_samplerID, -1, (sampler.GetName() + " sampler").c_str());

		if (!glIsSampler(params->m_samplerID))
		{
			LOG_ERROR("Texture sampler creation failed");
			SEAssert(glIsSampler(params->m_samplerID), "Texture sampler creation failed");
		}

		glSamplerParameteri(params->m_samplerID, GL_TEXTURE_WRAP_S, params->m_textureWrapS);
		glSamplerParameteri(params->m_samplerID, GL_TEXTURE_WRAP_T, params->m_textureWrapT);
		glSamplerParameteri(params->m_samplerID, GL_TEXTURE_WRAP_R, params->m_textureWrapR);

		glSamplerParameteri(params->m_samplerID, GL_TEXTURE_MIN_FILTER, params->m_textureMinFilter);
		glSamplerParameteri(params->m_samplerID, GL_TEXTURE_MAG_FILTER, params->m_textureMaxFilter);
		
		glSamplerParameterfv(params->m_samplerID, GL_TEXTURE_BORDER_COLOR, &sampler.GetSamplerParams().m_borderColor.x);
		
		// Finally, update the platform state:
		params->m_isCreated = true;

		// Note: We leave the sampler bound
	}


	void Sampler::Bind(re::Sampler const& sampler, uint32_t textureUnit)
	{
		opengl::Sampler::PlatformParams* params = sampler.GetPlatformParams()->As<opengl::Sampler::PlatformParams*>();

		glBindSampler(textureUnit, params->m_samplerID);
	}


	void Sampler::Destroy(re::Sampler& sampler)
	{
		opengl::Sampler::PlatformParams* params = sampler.GetPlatformParams()->As<opengl::Sampler::PlatformParams*>();
		glDeleteSamplers(1, &params->m_samplerID);
		params->m_samplerID = 0;
		params->m_isCreated = false;
	}
}