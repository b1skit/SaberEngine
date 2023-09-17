// © 2022 Adam Badke. All rights reserved.
#include <GL/glew.h>

#include "DebugConfiguration.h"
#include "Texture.h"
#include "Texture_Platform.h"
#include "Texture_OpenGL.h"


namespace
{
	bool GetFormatIsImageTextureCompatible(GLenum internalFormat)
	{
		// TODO: This list is not exhaustive, we should match the internal format with compatible formats:
		// https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBindImageTexture.xhtml
		// See also: glGetTextureParameter
		// https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGetTexParameter.xhtml

		switch (internalFormat)
		{
			case GL_RGBA32F:
			case GL_RGBA16F:
			case GL_RG32F:
			case GL_RG16F:
			case GL_R11F_G11F_B10F:
			case GL_R32F:
			case GL_R16F:
			case GL_RGBA32UI:
			case GL_RGBA16UI:
			case GL_RGB10_A2UI:
			case GL_RGBA8UI:
			case GL_RG32UI:
			case GL_RG16UI:
			case GL_RG8UI:
			case GL_R32UI:
			case GL_R16UI:
			case GL_R8UI:
			case GL_RGBA32I:
			case GL_RGBA16I:
			case GL_RGBA8I:
			case GL_RG32I:
			case GL_RG16I:
			case GL_RG8I:
			case GL_R32I:
			case GL_R16I:
			case GL_R8I:
			case GL_RGBA16:
			case GL_RGB10_A2:
			case GL_RGBA8:
			case GL_RG16:
			case GL_RG8:
			case GL_R16:
			case GL_R8:
			case GL_RGBA16_SNORM:
			case GL_RGBA8_SNORM:
			case GL_RG16_SNORM:
			case GL_RG8_SNORM:
			case GL_R16_SNORM:
			case GL_R8_SNORM:
				return true;
			default: 
			{
				return false;
			}
		}
		
	}
}

namespace opengl
{
	Texture::PlatformParams::PlatformParams(re::Texture const& texture)
		: m_textureID(0)
		, m_format(GL_RGBA)
		, m_internalFormat(GL_RGBA32F)
		, m_type(GL_FLOAT)
	{

		re::Texture::TextureParams const& texParams = texture.GetTextureParams();

		// Format:
		/****************/
		switch (texParams.m_format)
		{
		case re::Texture::Format::RGBA32F:
		{
			SEAssert("32-bit sRGB textures are not supported", texParams.m_colorSpace != re::Texture::ColorSpace::sRGB);
			m_format = GL_RGBA;
			m_internalFormat = GL_RGBA32F;
			m_type = GL_FLOAT;
		}
		break;
		case re::Texture::Format::RG32F:
		{
			SEAssert("32-bit sRGB textures are not supported", texParams.m_colorSpace != re::Texture::ColorSpace::sRGB);
			m_format = GL_RG;
			m_internalFormat = GL_RG32F;
			m_type = GL_FLOAT;
		}
		break;
		case re::Texture::Format::R32F:
		{
			SEAssert("32-bit sRGB textures are not supported", texParams.m_colorSpace != re::Texture::ColorSpace::sRGB);
			m_format = GL_R;
			m_internalFormat = GL_R32F;
			m_type = GL_FLOAT;
		}
		break;
		case re::Texture::Format::RGBA16F:
		{
			m_format = GL_RGBA;
			m_internalFormat = GL_RGBA16F;
			m_type = GL_HALF_FLOAT;
		}
		break;
		case re::Texture::Format::RG16F:
		{
			m_format = GL_RG;
			m_internalFormat = GL_RG16F;
			m_type = GL_HALF_FLOAT;
		}
		break;
		case re::Texture::Format::R16F:
		{
			m_format = GL_R;
			m_internalFormat = GL_R16F;
			m_type = GL_HALF_FLOAT;
		}
		break;
		case re::Texture::Format::RGBA8:
		{
			// Note: Alpha in GL_SRGB8_ALPHA8 is stored in linear color space, RGB are in sRGB color space
			m_format = GL_RGBA;
			m_type = GL_UNSIGNED_BYTE;
			switch (texParams.m_usage)
			{
			case re::Texture::Usage::Color:
			{
				m_internalFormat = texParams.m_colorSpace == re::Texture::ColorSpace::sRGB ? 
					GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM : GL_COMPRESSED_RGBA_BPTC_UNORM;
			}
			break;
			case re::Texture::Usage::ColorTarget:
			case re::Texture::Usage::SwapchainColorProxy:
			{
				m_internalFormat = texParams.m_colorSpace == re::Texture::ColorSpace::sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
			}
			break;
			default:
				SEAssertF("Invalid usage");
			}
		}
		break;
		case re::Texture::Format::RG8:
		{
			SEAssertF("Invalid/unsupported texture format");
		}
		break;
		case re::Texture::Format::R8:
		{
			SEAssertF("Invalid/unsupported texture format");
		}
		break;
		case re::Texture::Format::Depth32F:
		{
			m_format = GL_DEPTH_COMPONENT;
			m_internalFormat = GL_DEPTH_COMPONENT32F;
			m_type = GL_FLOAT;
		}
		break;
		default:
			SEAssertF("Invalid/unsupported texture format");
		}

		// Is this Texture compatible with compute workloads?
		m_formatIsImageTextureCompatible = GetFormatIsImageTextureCompatible(m_internalFormat);
	}



	Texture::PlatformParams::~PlatformParams()
	{
		glDeleteTextures(1, &m_textureID);
	}


	void opengl::Texture::Destroy(re::Texture& texture)
	{
		PlatformParams* params = texture.GetPlatformParams()->As<opengl::Texture::PlatformParams*>();
		if (!params)
		{
			return;
		}

		if (glIsTexture(params->m_textureID))
		{
			glDeleteTextures(1, &params->m_textureID);
			params->m_textureID = 0;
		}
	}


	void opengl::Texture::Bind(re::Texture const& texture, uint32_t textureUnit)
	{
		// Note: textureUnit is a binding point

		opengl::Texture::PlatformParams const* params =
			texture.GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

		// TODO: Support texture updates after modification
		SEAssert("Texture has been modified, and needs to be rebuffered", params->m_isDirty == false);

		glBindTextures(textureUnit, 1, &params->m_textureID);
		// TODO: Support binding an entire target set in a single call
	}


	void opengl::Texture::BindAsImageTexture(
		re::Texture const& texture, uint32_t textureUnit, uint32_t subresourceIdx, uint32_t accessMode)
	{
		SEAssert("Invalid access mode",
			accessMode == GL_READ_ONLY ||
			accessMode == GL_WRITE_ONLY ||
			accessMode == GL_READ_WRITE);

		opengl::Texture::PlatformParams const* texPlatParams =
			texture.GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

		SEAssert("Texture is not created", texPlatParams->m_isCreated);

		SEAssert("Texture is not marked for compute usage",
			(texture.GetTextureParams().m_usage & re::Texture::Usage::ComputeTarget));

		SEAssert("Format is not compatible. Note: We currently don't check for non-exact but compatible formats, "
			"but should. See Texture_OpenGL.cpp::GetFormatIsImageTextureCompatible",
			texPlatParams->m_formatIsImageTextureCompatible);

		glBindImageTexture(
			textureUnit,						// unit: Index to bind to
			texPlatParams->m_textureID,			// texture: Name of the texture being bound
			subresourceIdx,	// level: Subresource index being bound
			GL_TRUE,							// layered: Use layered binding? Binds the entire 1/2/3D array if true
			0,									// layer: Layer to bind. Ignored if layered == GL_TRUE
			accessMode,							// access: Type of access that will be performed
			texPlatParams->m_internalFormat);	// format: Internal format	
	}


	void opengl::Texture::Create(re::Texture& texture)
	{
		opengl::Texture::PlatformParams* params = texture.GetPlatformParams()->As<opengl::Texture::PlatformParams*>();
		SEAssert("Attempting to create a texture that already exists", 
			!glIsTexture(params->m_textureID) && !params->m_isCreated);
		params->m_isCreated = true;

		LOG("Creating & buffering texture: \"%s\"", texture.GetName().c_str());

		// Ensure our texture is correctly configured:
		re::Texture::TextureParams const& texParams = texture.GetTextureParams();
		SEAssert("Texture has a bad configuration", 
			texParams.m_dimension != re::Texture::Dimension::TextureCubeMap || texParams.m_faces == 6);

		// Generate textureID names. Note: We must call glBindTexture immediately after to associate the name with 
		// a texture. It will not have the correct dimensionality until this is done
		glGenTextures(1, &params->m_textureID);
		switch (texParams.m_dimension)
		{
		case re::Texture::Dimension::Texture2D:
		{
			glBindTexture(GL_TEXTURE_2D, params->m_textureID);
		}
		break;
		case re::Texture::Dimension::TextureCubeMap:
		{
			glBindTexture(GL_TEXTURE_CUBE_MAP, params->m_textureID);
		}
		break;
		default:
			SEAssertF("Invalid texture dimension");
		}
		SEAssert("OpenGL failed to generate new texture name", glIsTexture(params->m_textureID) == GL_TRUE);

		// RenderDoc object name:
		glObjectLabel(GL_TEXTURE, params->m_textureID, -1, texture.GetName().c_str());

		// Specify the texture storage:
		const uint32_t width = texture.Width();
		const uint32_t height = texture.Height();

		glTextureStorage2D(
			params->m_textureID,
			texture.GetNumMips(),
			params->m_internalFormat,
			width,
			height);

		// Upload data (if any) to the GPU:
		if ((texParams.m_usage & re::Texture::Usage::Color) && texture.HasInitialData())
		{
			for (uint32_t i = 0; i < texParams.m_faces; i++)
			{
				void* data = texture.GetTexelData(i);
				SEAssert("Color target must have data to buffer", data);				

				if (texParams.m_dimension == re::Texture::Dimension::TextureCubeMap)
				{
					glTextureSubImage3D(
						params->m_textureID,
						0, // Level: Mip level
						0, // xoffset
						0, // yoffset
						i, // zoffset: Target face
						width,
						height,
						texParams.m_faces,	// depth
						params->m_format,	// format
						params->m_type,		// type
						data);				// void* data. Nullptr for render targets
				}
				else
				{
					glTextureSubImage2D(
						params->m_textureID,
						0, // Level: Mip level
						0, // xoffset
						0, // yoffset
						width,
						height,
						params->m_format,	// format
						params->m_type,		// type
						data);				// void* data. Nullptr for render targets
				}
			}
		}

		// Create mips:
		opengl::Texture::GenerateMipMaps(texture);

		params->m_isDirty = false;

		// Note: We leave the texture and samplers bound
	}


	void opengl::Texture::GenerateMipMaps(re::Texture const& texture)
	{
		opengl::Texture::PlatformParams const* params =
			texture.GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

		if (texture.GetTextureParams().m_mipMode == re::Texture::MipMode::AllocateGenerate)
		{
			glGenerateTextureMipmap(params->m_textureID);
		}
		else
		{
			const GLint maxLevel = GL_TEXTURE_MAX_LEVEL;
			glTextureParameteriv(params->m_textureID, GL_TEXTURE_MAX_LEVEL, &maxLevel);
		}
	}
}