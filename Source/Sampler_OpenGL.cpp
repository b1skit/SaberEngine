// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core\Assert.h"
#include "Core\Util\CastUtils.h"
#include "Sampler_OpenGL.h"
#include "Sampler_Platform.h"
#include "SysInfo_OpenGL.h"


namespace
{
	constexpr GLenum GetOpenGLMinFilter(re::Sampler::FilterMode filterMode)
	{
		// As per https://www.khronos.org/opengl/wiki/Sampler_Object#Anisotropic_filtering, we combine anisotropic 
		// filtering with a GL_LINEAR_MIPMAP_LINEAR minification filter in all cases
		switch (filterMode)
		{
		case re::Sampler::FilterMode::MIN_MAG_MIP_POINT: return GL_NEAREST;
		case re::Sampler::FilterMode::MIN_MAG_POINT_MIP_LINEAR: return GL_NEAREST_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MIN_POINT_MAG_LINEAR_MIP_POINT: return GL_NEAREST_MIPMAP_NEAREST;
		case re::Sampler::FilterMode::MIN_POINT_MAG_MIP_LINEAR: return GL_NEAREST_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MIN_LINEAR_MAG_MIP_POINT: return GL_LINEAR_MIPMAP_NEAREST;
		case re::Sampler::FilterMode::MIN_LINEAR_MAG_POINT_MIP_LINEAR: return GL_LINEAR_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MIN_MAG_LINEAR_MIP_POINT: return GL_LINEAR_MIPMAP_NEAREST;
		case re::Sampler::FilterMode::MIN_MAG_MIP_LINEAR: return GL_LINEAR_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MIN_MAG_ANISOTROPIC_MIP_POINT: return GL_LINEAR_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::ANISOTROPIC: return GL_LINEAR_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::COMPARISON_MIN_MAG_MIP_POINT: return GL_NEAREST;
		case re::Sampler::FilterMode::COMPARISON_MIN_MAG_POINT_MIP_LINEAR: return GL_NEAREST_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT: return GL_NEAREST_MIPMAP_NEAREST;
		case re::Sampler::FilterMode::COMPARISON_MIN_POINT_MAG_MIP_LINEAR: return GL_NEAREST_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::COMPARISON_MIN_LINEAR_MAG_MIP_POINT: return GL_LINEAR_MIPMAP_NEAREST;
		case re::Sampler::FilterMode::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR: return GL_LINEAR_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::COMPARISON_MIN_MAG_LINEAR_MIP_POINT: return GL_LINEAR_MIPMAP_NEAREST;
		case re::Sampler::FilterMode::COMPARISON_MIN_MAG_MIP_LINEAR: return GL_LINEAR_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::COMPARISON_MIN_MAG_ANISOTROPIC_MIP_POINT: return GL_LINEAR_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::COMPARISON_ANISOTROPIC: return GL_LINEAR_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MINIMUM_MIN_MAG_MIP_POINT: return GL_NEAREST;
		case re::Sampler::FilterMode::MINIMUM_MIN_MAG_POINT_MIP_LINEAR: return GL_NEAREST_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT: return GL_NEAREST_MIPMAP_NEAREST;
		case re::Sampler::FilterMode::MINIMUM_MIN_POINT_MAG_MIP_LINEAR: return GL_NEAREST_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MINIMUM_MIN_LINEAR_MAG_MIP_POINT: return GL_LINEAR_MIPMAP_NEAREST;
		case re::Sampler::FilterMode::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR: return GL_LINEAR_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MINIMUM_MIN_MAG_LINEAR_MIP_POINT: return GL_LINEAR_MIPMAP_NEAREST;
		case re::Sampler::FilterMode::MINIMUM_MIN_MAG_MIP_LINEAR: return GL_LINEAR_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MINIMUM_MIN_MAG_ANISOTROPIC_MIP_POINT: return GL_LINEAR_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MINIMUM_ANISOTROPIC: return GL_LINEAR_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MAXIMUM_MIN_MAG_MIP_POINT: return GL_NEAREST;
		case re::Sampler::FilterMode::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR: return GL_NEAREST_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT: return GL_NEAREST_MIPMAP_NEAREST;
		case re::Sampler::FilterMode::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR: return GL_NEAREST_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT: return GL_LINEAR_MIPMAP_NEAREST;
		case re::Sampler::FilterMode::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR: return GL_LINEAR_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT: return GL_LINEAR_MIPMAP_NEAREST;
		case re::Sampler::FilterMode::MAXIMUM_MIN_MAG_MIP_LINEAR: return GL_LINEAR_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MAXIMUM_MIN_MAG_ANISOTROPIC_MIP_POINT: return GL_LINEAR_MIPMAP_LINEAR;
		case re::Sampler::FilterMode::MAXIMUM_ANISOTROPIC: return GL_LINEAR_MIPMAP_LINEAR;
		}
		return GL_LINEAR_MIPMAP_LINEAR; // Return a reasonable default to suppress compiler warning
	}


	constexpr GLenum GetOpenGLMagFilter(re::Sampler::FilterMode filterMode)
	{
		switch (filterMode)
		{
		case re::Sampler::FilterMode::MIN_MAG_MIP_POINT: return GL_NEAREST;
		case re::Sampler::FilterMode::MIN_MAG_POINT_MIP_LINEAR: return GL_NEAREST;
		case re::Sampler::FilterMode::MIN_POINT_MAG_LINEAR_MIP_POINT: return GL_LINEAR;
		case re::Sampler::FilterMode::MIN_POINT_MAG_MIP_LINEAR: return GL_LINEAR;
		case re::Sampler::FilterMode::MIN_LINEAR_MAG_MIP_POINT: return GL_NEAREST;
		case re::Sampler::FilterMode::MIN_LINEAR_MAG_POINT_MIP_LINEAR: return GL_NEAREST;
		case re::Sampler::FilterMode::MIN_MAG_LINEAR_MIP_POINT: return GL_LINEAR;
		case re::Sampler::FilterMode::MIN_MAG_MIP_LINEAR: return GL_LINEAR;
		case re::Sampler::FilterMode::MIN_MAG_ANISOTROPIC_MIP_POINT: return GL_LINEAR;
		case re::Sampler::FilterMode::ANISOTROPIC: return GL_LINEAR;
		case re::Sampler::FilterMode::COMPARISON_MIN_MAG_MIP_POINT: return GL_NEAREST;
		case re::Sampler::FilterMode::COMPARISON_MIN_MAG_POINT_MIP_LINEAR: return GL_NEAREST;
		case re::Sampler::FilterMode::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT: return GL_LINEAR;
		case re::Sampler::FilterMode::COMPARISON_MIN_POINT_MAG_MIP_LINEAR: return GL_LINEAR;
		case re::Sampler::FilterMode::COMPARISON_MIN_LINEAR_MAG_MIP_POINT: return GL_NEAREST;
		case re::Sampler::FilterMode::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR: return GL_NEAREST;
		case re::Sampler::FilterMode::COMPARISON_MIN_MAG_LINEAR_MIP_POINT: return GL_LINEAR;
		case re::Sampler::FilterMode::COMPARISON_MIN_MAG_MIP_LINEAR: return GL_LINEAR;
		case re::Sampler::FilterMode::COMPARISON_MIN_MAG_ANISOTROPIC_MIP_POINT: return GL_LINEAR;
		case re::Sampler::FilterMode::COMPARISON_ANISOTROPIC: return GL_LINEAR;
		case re::Sampler::FilterMode::MINIMUM_MIN_MAG_MIP_POINT: return GL_NEAREST;
		case re::Sampler::FilterMode::MINIMUM_MIN_MAG_POINT_MIP_LINEAR: return GL_NEAREST;
		case re::Sampler::FilterMode::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT: return GL_LINEAR;
		case re::Sampler::FilterMode::MINIMUM_MIN_POINT_MAG_MIP_LINEAR: return GL_LINEAR;
		case re::Sampler::FilterMode::MINIMUM_MIN_LINEAR_MAG_MIP_POINT: return GL_NEAREST;
		case re::Sampler::FilterMode::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR: return GL_NEAREST;
		case re::Sampler::FilterMode::MINIMUM_MIN_MAG_LINEAR_MIP_POINT: return GL_LINEAR;
		case re::Sampler::FilterMode::MINIMUM_MIN_MAG_MIP_LINEAR: return GL_LINEAR;
		case re::Sampler::FilterMode::MINIMUM_MIN_MAG_ANISOTROPIC_MIP_POINT: return GL_LINEAR;
		case re::Sampler::FilterMode::MINIMUM_ANISOTROPIC: return GL_LINEAR;
		case re::Sampler::FilterMode::MAXIMUM_MIN_MAG_MIP_POINT: return GL_NEAREST;
		case re::Sampler::FilterMode::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR: return GL_NEAREST;
		case re::Sampler::FilterMode::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT: return GL_LINEAR;
		case re::Sampler::FilterMode::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR: return GL_LINEAR;
		case re::Sampler::FilterMode::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT: return GL_NEAREST;
		case re::Sampler::FilterMode::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR: return GL_NEAREST;
		case re::Sampler::FilterMode::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT: return GL_LINEAR;
		case re::Sampler::FilterMode::MAXIMUM_MIN_MAG_MIP_LINEAR: return GL_LINEAR;
		case re::Sampler::FilterMode::MAXIMUM_MIN_MAG_ANISOTROPIC_MIP_POINT: return GL_LINEAR;
		case re::Sampler::FilterMode::MAXIMUM_ANISOTROPIC: return GL_LINEAR;
		}
		return GL_LINEAR; // Return a reasonable default to suppress compiler warning
	}


	constexpr GLenum GetOpenGLEdgeMode(re::Sampler::EdgeMode edgeMode)
	{
		switch (edgeMode)
		{
		case re::Sampler::EdgeMode::Wrap: return GL_REPEAT;
		case re::Sampler::EdgeMode::Mirror: return GL_MIRRORED_REPEAT;
		case re::Sampler::EdgeMode::Clamp: return GL_CLAMP_TO_EDGE;
		case re::Sampler::EdgeMode::Border: return GL_CLAMP_TO_BORDER;
		case re::Sampler::EdgeMode::MirrorOnce: return GL_MIRROR_CLAMP_TO_EDGE;
		}
		return GL_REPEAT; // Return a reasonable default to suppress compiler warning
	}


	constexpr GLenum GetOpenGLComparisonFunc(re::Sampler::ComparisonFunc comparisonFunc)
	{
		switch (comparisonFunc)
		{
			case re::Sampler::ComparisonFunc::None: return GL_ALWAYS;
			case re::Sampler::ComparisonFunc::Never: return GL_NEVER;
			case re::Sampler::ComparisonFunc::Less: return GL_LESS;
			case re::Sampler::ComparisonFunc::Equal: return GL_EQUAL;
			case re::Sampler::ComparisonFunc::LessEqual: return GL_LEQUAL;
			case re::Sampler::ComparisonFunc::Greater: return GL_GREATER;
			case re::Sampler::ComparisonFunc::NotEqual: return GL_NOTEQUAL;
			case re::Sampler::ComparisonFunc::GreaterEqual: return GL_GEQUAL;
			case re::Sampler::ComparisonFunc::Always: return GL_ALWAYS;
		}
		return GL_ALWAYS;
	}


	void const* GetBorderColor(re::Sampler::SamplerDesc samplerDesc)
	{
		static const glm::vec4 k_transparentBlack = glm::vec4(0.f);
		static const glm::vec4 k_opaqueBlack = glm::vec4(0.f, 0.f, 0.f, 1.f);
		static const glm::vec4 k_opaqueWhite = glm::vec4(0.f, 0.f, 0.f, 1.f);
		static const glm::uvec4 k_opaqueBlackUInt = glm::uvec4(0, 0, 0, 255);
		static const glm::uvec4 k_opaqueWhiteUInt = glm::uvec4(255, 255, 255, 255);

		switch (samplerDesc.m_borderColor)
		{
		case re::Sampler::BorderColor::TransparentBlack: return &k_transparentBlack.r;
		case re::Sampler::BorderColor::OpaqueBlack: return &k_opaqueBlack.r;
		case re::Sampler::BorderColor::OpaqueWhite: return &k_opaqueWhite.r;
		case re::Sampler::BorderColor::OpaqueBlack_UInt: return &k_opaqueBlackUInt.r;
		case re::Sampler::BorderColor::OpaqueWhite_UInt: return &k_opaqueWhiteUInt.r;
		}
		return &k_transparentBlack.r;
	}
}


namespace opengl
{
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

		re::Sampler::SamplerDesc const& samplerDesc = sampler.GetSamplerDesc();

		// Populate our sampler parameters from our SE SamplerDesc:
		params->m_textureMinFilter = GetOpenGLMinFilter(samplerDesc.m_filterMode);
		glSamplerParameteri(params->m_samplerID, GL_TEXTURE_MIN_FILTER, params->m_textureMinFilter);

		params->m_textureMagFilter = GetOpenGLMagFilter(samplerDesc.m_filterMode);
		glSamplerParameteri(params->m_samplerID, GL_TEXTURE_MAG_FILTER, params->m_textureMagFilter);

		params->m_textureWrapS = GetOpenGLEdgeMode(samplerDesc.m_edgeModeU);
		glSamplerParameteri(params->m_samplerID, GL_TEXTURE_WRAP_S, params->m_textureWrapS);

		params->m_textureWrapT = GetOpenGLEdgeMode(samplerDesc.m_edgeModeV);
		glSamplerParameteri(params->m_samplerID, GL_TEXTURE_WRAP_T, params->m_textureWrapT);

		params->m_textureWrapR = GetOpenGLEdgeMode(samplerDesc.m_edgeModeW);
		glSamplerParameteri(params->m_samplerID, GL_TEXTURE_WRAP_R, params->m_textureWrapR);

		glTextureParameterf(params->m_samplerID, GL_TEXTURE_LOD_BIAS, samplerDesc.m_mipLODBias);

		SEAssert(samplerDesc.m_maxAnisotropy <= util::CheckedCast<uint32_t>(opengl::SysInfo::GetMaxAnisotropy()),
			"Max anisotropy exceeds what the system is capable of");
		glTextureParameteri(params->m_samplerID, GL_TEXTURE_MAX_ANISOTROPY, samplerDesc.m_maxAnisotropy);
		
		params->m_comparisonFunc = GetOpenGLComparisonFunc(samplerDesc.m_comparisonFunc);
		if (samplerDesc.m_comparisonFunc == re::Sampler::ComparisonFunc::None)
		{
			glSamplerParameteri(params->m_samplerID, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		}
		else
		{
			glSamplerParameteri(params->m_samplerID, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glSamplerParameteri(params->m_samplerID, GL_TEXTURE_COMPARE_FUNC, params->m_comparisonFunc);
		}

		if (samplerDesc.m_borderColor == re::Sampler::BorderColor::OpaqueBlack_UInt ||
			samplerDesc.m_borderColor == re::Sampler::BorderColor::OpaqueWhite_UInt)
		{
			glSamplerParameteriv(params->m_samplerID, 
				GL_TEXTURE_BORDER_COLOR, 
				static_cast<GLint const*>(GetBorderColor(samplerDesc)));
		}
		else
		{
			glSamplerParameterfv(params->m_samplerID, 
				GL_TEXTURE_BORDER_COLOR, 
				static_cast<GLfloat const*>(GetBorderColor(samplerDesc)));
		}
		
		glTextureParameterf(params->m_samplerID, GL_TEXTURE_MIN_LOD, samplerDesc.m_minLOD);
		glTextureParameterf(params->m_samplerID, GL_TEXTURE_MAX_LOD, samplerDesc.m_maxLOD);

		
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