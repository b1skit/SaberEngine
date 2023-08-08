// © 2022 Adam Badke. All rights reserved.
#include <GL/glew.h>

#include "DebugConfiguration.h"
#include "Texture.h"
#include "Texture_Platform.h"
#include "Texture_OpenGL.h"



namespace opengl
{
	Texture::PlatformParams::PlatformParams(re::Texture::TextureParams const& texParams) :
		m_textureID(0),
		m_texTarget(GL_TEXTURE_2D),
		m_format(GL_RGBA),
		m_internalFormat(GL_RGBA32F),
		m_type(GL_FLOAT)
	{
		// Dimension:
		/************/
		switch (texParams.m_dimension)
		{
		case re::Texture::Dimension::Texture2D:
		{
			m_texTarget = GL_TEXTURE_2D;
		}
		break;
		case re::Texture::Dimension::TextureCubeMap:
		{
			m_texTarget = GL_TEXTURE_CUBE_MAP;
		}
		break;
		default:
			SEAssertF("Invalid/unsupported texture dimension");
		}


		// Format:
		/****************/
		switch (texParams.m_format)
		{
		case re::Texture::Format::RGBA32F:
		{
			m_format = GL_RGBA;
			m_internalFormat = GL_RGBA32F;
			m_type = GL_FLOAT;
		}
		break;
		case re::Texture::Format::RG32F:
		{
			m_format = GL_RG;
			m_internalFormat = GL_RG32F;
			m_type = GL_FLOAT;
		}
		break;
		case re::Texture::Format::R32F:
		{
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


	void opengl::Texture::Bind(re::Texture& texture, uint32_t textureUnit)
	{
		// Note: textureUnit is a binding point

		opengl::Texture::PlatformParams const* params =
			texture.GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

		// TODO: Support texture updates after modification
		SEAssert("Texture has been modified, and needs to be rebuffered", params->m_isDirty == false);

		glBindTextures(textureUnit, 1, &params->m_textureID);
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
		SEAssert("Texture has a bad configuration", texParams.m_faces == 1 ||
			(texParams.m_faces == 6 && texParams.m_dimension == re::Texture::Dimension::TextureCubeMap));

		// Generate textureID names. Note: We must call glBindTexture immediately after to associate the name with 
		// a texture. It will not have the correct dimensionality until this is done
		glGenTextures(1, &params->m_textureID);
		glBindTexture(params->m_texTarget, params->m_textureID);
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
		if ((texParams.m_usage & re::Texture::Usage::Color))
		{
			for (uint32_t i = 0; i < texParams.m_faces; i++)
			{
				SEAssert("Color target must have data to buffer",
					texture.GetTexels().size() ==
					(texParams.m_faces * texParams.m_width * texParams.m_height * re::Texture::GetNumBytesPerTexel(texParams.m_format)));

				void* data = (void*)texture.GetTexel(0, 0, i);

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


	void opengl::Texture::GenerateMipMaps(re::Texture& texture)
	{
		opengl::Texture::PlatformParams const* params =
			texture.GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

		if (texture.GetTextureParams().m_useMIPs)
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