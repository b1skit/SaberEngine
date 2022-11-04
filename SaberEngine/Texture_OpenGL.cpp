#include <memory>

#include <GL/glew.h>

#include "DebugConfiguration.h"
#include "Texture.h"
#include "Texture_Platform.h"
#include "Texture_OpenGL.h"



namespace opengl
{
	// Ctor: Maps SaberEngine -> OpenGL texture params:
	Texture::PlatformParams::PlatformParams(gr::Texture::TextureParams const& texParams) :
		m_textureID(0),
		m_texTarget(GL_TEXTURE_2D),
		m_format(GL_RGBA),
		m_internalFormat(GL_RGBA32F),
		m_type(GL_FLOAT),
		m_clearColor(0, 0, 0, 1)
	{
		// Dimension:
		/************/
		switch (texParams.m_texDimension)
		{
		case gr::Texture::TextureDimension::Texture2D:
		{
			m_texTarget = GL_TEXTURE_2D;
		}
		break;
		case gr::Texture::TextureDimension::TextureCubeMap:
		{
			m_texTarget = GL_TEXTURE_CUBE_MAP;
		}
		break;
		default:
			SEAssertF("Invalid/unsupported texture dimension");
		}


		// TextureFormat:
		/****************/
		switch (texParams.m_texFormat)
		{
		case gr::Texture::TextureFormat::RGBA32F:
		{
			m_format = GL_RGBA;
			m_internalFormat = GL_RGBA32F;
			m_type = GL_FLOAT;
		}
		break;
		case gr::Texture::TextureFormat::RGB32F:
		{
			m_format = GL_RGB;
			m_internalFormat = GL_RGB32F;
			m_type = GL_FLOAT;
		}
		break;
		case gr::Texture::TextureFormat::RG32F:
		{
			m_format = GL_RG;
			m_internalFormat = GL_RG32F;
			m_type = GL_FLOAT;
		}
		break;
		case gr::Texture::TextureFormat::R32F:
		{
			m_format = GL_R;
			m_internalFormat = GL_R32F;
			m_type = GL_FLOAT;
		}
		break;
		case gr::Texture::TextureFormat::RGBA16F:
		{
			m_format = GL_RGBA;
			m_internalFormat = GL_RGBA16F;
			m_type = GL_HALF_FLOAT;
		}
		break;
		case gr::Texture::TextureFormat::RGB16F:
		{
			m_format = GL_RGB;
			m_internalFormat = GL_RGB16F;
			m_type = GL_HALF_FLOAT;
		}
		break;
		case gr::Texture::TextureFormat::RG16F:
		{
			m_format = GL_RG;
			m_internalFormat = GL_RG16F;
			m_type = GL_HALF_FLOAT;
		}
		break;
		case gr::Texture::TextureFormat::R16F:
		{
			m_format = GL_R;
			m_internalFormat = GL_R16F;
			m_type = GL_HALF_FLOAT;
		}
		break;
		case gr::Texture::TextureFormat::RGBA8:
		{
			m_format = GL_RGBA;
			m_internalFormat = 
				texParams.m_texColorSpace == gr::Texture::TextureColorSpace::sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
			// Note: Alpha in GL_SRGB8_ALPHA8 is stored in linear color space, RGB are in sRGB color space
			m_type = GL_UNSIGNED_BYTE;
		}
		break;
		case gr::Texture::TextureFormat::RGB8:
		{
			m_format = GL_RGB;
			m_internalFormat = 
				texParams.m_texColorSpace == gr::Texture::TextureColorSpace::sRGB ? GL_SRGB8 : GL_RGB8;		
			m_type = GL_UNSIGNED_BYTE;
		}
		break;
		case gr::Texture::TextureFormat::RG8:
		{
			SEAssertF("Invalid/unsupported texture format");
		}
		break;
		case gr::Texture::TextureFormat::R8:
		{
			SEAssertF("Invalid/unsupported texture format");
		}
		break;
		case gr::Texture::TextureFormat::Depth32F:
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


	void opengl::Texture::Destroy(gr::Texture& texture)
	{
		PlatformParams const* const params =
			dynamic_cast<opengl::Texture::PlatformParams*>(texture.GetPlatformParams());

		if (!params)
		{
			return;
		}

		if (glIsTexture(params->m_textureID))
		{
			glDeleteTextures(1, &params->m_textureID);
		}
	}


	void opengl::Texture::Bind(gr::Texture const& texture, uint32_t textureUnit, bool doBind/*= true*/)
	{
		// TODO: Is there a way to avoid needing to pass textureUnit?
		// textureUnit is a target, ie. GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP, etc

		opengl::Texture::PlatformParams const* const params =
			dynamic_cast<opengl::Texture::PlatformParams const*>(texture.GetPlatformParams());

		if (doBind)
		{
			glBindTextures(textureUnit, 1, &params->m_textureID);
		}
		else
		{
			glBindTextures(textureUnit, 1, 0);
		}
	}


	void opengl::Texture::Create(gr::Texture& texture)
	{
		LOG("Creating & buffering texture: \"%s\"", texture.GetName().c_str());

		// Create the platform-specific parameters object:
		platform::Texture::PlatformParams::CreatePlatformParams(texture);

		// Get our platform params now that the texture has been created:
		PlatformParams* const params =
			dynamic_cast<opengl::Texture::PlatformParams* const>(texture.GetPlatformParams());
		SEAssert("Attempting to create a texture that already exists", !glIsTexture(params->m_textureID));

		// Generate textureID names. Note: We must call glBindTexture immediately after to associate the name with 
		// a texture. It will not have the correct dimensionality until this is done
		glGenTextures(1, &params->m_textureID);
		glBindTexture(params->m_texTarget, params->m_textureID);

		// RenderDoc object name:
		glObjectLabel(GL_TEXTURE, params->m_textureID, -1, texture.GetName().c_str());

		SEAssert("OpenGL failed to generate new texture name. Texture buffering failed", 
			glIsTexture(params->m_textureID) == GL_TRUE);

		// Ensure our texture is correctly configured:
		gr::Texture::TextureParams const& texParams = texture.GetTextureParams();
		SEAssert("Texture has a bad configuration", texParams.m_faces == 1 ||
			(texParams.m_faces == 6 && texParams.m_texDimension == gr::Texture::TextureDimension::TextureCubeMap));

		// Buffer the texture data:
		const uint32_t width = texture.Width();
		const uint32_t height = texture.Height();

		// Upload data (if any) to the GPU:
		for (uint32_t i = 0; i < texParams.m_faces; i++)
		{
			// Get the image data pointer; for render targets, this is nullptr
			void* data = nullptr;
			if (texParams.m_texUse == gr::Texture::TextureUse::Color)
			{
				SEAssert("Color target must have data to buffer", 
					texture.Texels().size() == 
					(texParams.m_faces * texParams.m_width * texParams.m_height * gr::Texture::GetNumBytesPerTexel(texParams.m_texFormat)));

				data = (void*)texture.GetTexel(0, 0, i);
			}

			GLenum target;
			if (texParams.m_texDimension == gr::Texture::TextureDimension::TextureCubeMap)
			{
				target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
				// TODO: Switch to a generic specification/buffering functionality, using GL_TEXTURE_CUBE_MAP for 
				// m_texTarget instead of GL_TEXTURE_CUBE_MAP_POSITIVE_X
				// // -> Doing a similar thing in TextureTargetSet
				// -> Currently fails if we use GL_TEXTURE_CUBE_MAP...

				// https://www.reddit.com/r/opengl/comments/556zac/how_to_create_cubemap_with_direct_state_access/
				// Specify storage with glTextureStorage2D: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexStorage2D.xhtml
				// Buffer each face with glTextureSubImage3D
			}
			else
			{
				target = params->m_texTarget;
			}

			// Compute the byte alignment for w.r.t to the image dimensions. Allows RGB8 textures (3x 1-byte channels)
			// to be correctly buffered. Default alignment is 4.
			GLint byteAlignment = 8;
			while (texture.Width() % byteAlignment != 0)
			{
				byteAlignment /= 2; // 8, 4, 2, 1
			}
			SEAssert("Invalid byte alignment",
				byteAlignment == 8 || byteAlignment == 4 || byteAlignment == 2 || byteAlignment == 1);

			// Set the byte alignment for the start of each image row in memory:
			glPixelStorei(GL_UNPACK_ALIGNMENT, byteAlignment);

			// Specify the texture:
			glTexImage2D(					// Specifies a 2D texture
				target + (GLenum)i,			// target: GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP_POSITIVE_X, etc
				0,							// mip level
				params->m_internalFormat,	// internal format
				width,						// width
				height,						// height
				0,							// border
				params->m_format,			// format
				params->m_type,				// type
				data);						// void* data. Nullptr for render targets
		}

		// Create mips:
		opengl::Texture::GenerateMipMaps(texture);

		// Note: We leave the texture and samplers bound
	}


	void opengl::Texture::GenerateMipMaps(gr::Texture& texture)
	{
		opengl::Texture::PlatformParams const* const params =
			dynamic_cast<opengl::Texture::PlatformParams*>(texture.GetPlatformParams());

		if (texture.GetTextureParams().m_useMIPs == false)
		{
			const GLint maxLevel = GL_TEXTURE_MAX_LEVEL;
			glTextureParameteriv(params->m_textureID, GL_TEXTURE_MAX_LEVEL, &maxLevel);
			return;
		}

		glGenerateTextureMipmap(params->m_textureID);
	}


	platform::Texture::UVOrigin opengl::Texture::GetUVOrigin()
	{
		return platform::Texture::UVOrigin::BottomLeft;
	}
}