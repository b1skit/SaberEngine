#include <memory>

#include <GL/glew.h>

#include "BuildConfiguration.h"
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
		m_textureWrapS(GL_REPEAT),
		m_textureWrapT(GL_REPEAT),
		m_textureWrapR(GL_REPEAT),
		m_textureMinFilter(GL_NEAREST_MIPMAP_LINEAR),
		m_textureMaxFilter(GL_LINEAR),
		m_samplerID(0),
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
			assert("Invalid/unsupported texture dimension" && false);
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
			m_type = GL_FLOAT;
		}
		break;
		case gr::Texture::TextureFormat::RGB16F:
		{
			m_format = GL_RGB;
			m_internalFormat = GL_RGB16F;
			m_type = GL_FLOAT;
		}
		break;
		case gr::Texture::TextureFormat::RG16F:
		{
			m_format = GL_RG;
			m_internalFormat = GL_RG16F;
			m_type = GL_FLOAT;
		}
		break;
		case gr::Texture::TextureFormat::R16F:
		{
			m_format = GL_R;
			m_internalFormat = GL_R16F;
			m_type = GL_FLOAT;
		}
		break;
		case gr::Texture::TextureFormat::RGBA8:
		{
			m_format = GL_RGBA;
			m_internalFormat = 
				texParams.m_texColorSpace == gr::Texture::TextureColorSpace::sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
			m_type = GL_FLOAT;
		}
		break;
		case gr::Texture::TextureFormat::RGB8:
		{
			m_format = GL_RGB;
			m_internalFormat = GL_SRGB8;
			m_type = GL_FLOAT;
		}
		break;
		case gr::Texture::TextureFormat::RG8:
		{
			assert("Invalid/unsupported texture format" && false);
		}
		break;
		case gr::Texture::TextureFormat::R8:
		{
			assert("Invalid/unsupported texture format" && false);
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
			assert("Invalid/unsupported texture format" && false);
		}

		// Minification filter:
		/*********************/
		switch (texParams.m_texMinMode)
		{
		case gr::Texture::TextureMinFilter::Nearest:
		{
			m_textureMinFilter = GL_NEAREST;
		}
		break;
		case gr::Texture::TextureMinFilter::NearestMipMapLinear:
		{
			m_textureMinFilter = GL_NEAREST_MIPMAP_LINEAR;
		}
		break;
		case gr::Texture::TextureMinFilter::Linear:
		{
			m_textureMinFilter = GL_LINEAR;
		}
		break;
		case gr::Texture::TextureMinFilter::LinearMipMapLinear:
		{
			m_textureMinFilter = GL_LINEAR_MIPMAP_LINEAR;
		}
		break;
		default:
			assert("Invalid/unsupported texture min mode" && false);
		}

		// Maxification filter:
		/*********************/
		switch (texParams.m_texMaxMode)
		{
		case gr::Texture::TextureMaxFilter::Nearest:
		{
			m_textureMaxFilter = GL_NEAREST;
		}
		break;
		case gr::Texture::TextureMaxFilter::Linear:
		{
			m_textureMaxFilter = GL_LINEAR;
		}

		break;
		default:
			assert("Invalid/unsupported texture max mode" && false);
		}

		// Sampler mode:
		/***************/
		switch (texParams.m_texSamplerMode)
		{
		case gr::Texture::TextureSamplerMode::Wrap:
		{
			m_textureWrapS = GL_REPEAT;
			m_textureWrapT = GL_REPEAT;
			m_textureWrapR = GL_REPEAT;
		}
		break;
		case gr::Texture::TextureSamplerMode::Mirrored:
		{
			m_textureWrapS = GL_MIRRORED_REPEAT;
			m_textureWrapT = GL_MIRRORED_REPEAT;
			m_textureWrapR = GL_MIRRORED_REPEAT;			
		}
		break;
		case gr::Texture::TextureSamplerMode::Clamp:
		{
			m_textureWrapS = GL_CLAMP_TO_EDGE;
			m_textureWrapT = GL_CLAMP_TO_EDGE;
			m_textureWrapR = GL_CLAMP_TO_EDGE;
		}
		break;
		default:
			assert("Invalid/unsupported texture max mode" && false);
		}
	}



	Texture::PlatformParams::~PlatformParams()
	{
		if (glIsTexture(m_textureID))
		{
			LOG_WARNING("DELETING TEXTUREID FROM PLATFORM PARAMS!!!!!!!!!!!");

			glDeleteTextures(1, &m_textureID);
		}

		if (glIsSampler(m_samplerID))
		{
			LOG_WARNING("DELETING SAMPLERID FROM PLATFORM PARAMS!!!!!!!!!!!");

			glDeleteSamplers(1, &m_samplerID);
		}
	}


	void opengl::Texture::Destroy(gr::Texture& texture)
	{
		PlatformParams const* const params =
			dynamic_cast<opengl::Texture::PlatformParams*>(texture.GetPlatformParams());

		// Nothing to delete if the texture wasn't created
		if (!params)
		{
			return;
		}

		if (glIsTexture(params->m_textureID))
		{
			glDeleteTextures(1, &params->m_textureID);
		}

		glDeleteSamplers(1, &params->m_samplerID);
	}


	void opengl::Texture::Bind(gr::Texture const& texture, uint32_t textureUnit, bool doBind/*= true*/)
	{
		// TODO: Is there a way to avoid needing to pass textureUnit?
		// textureUnit is a target, ie. GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP, etc

		opengl::Texture::PlatformParams const* const params =
			dynamic_cast<opengl::Texture::PlatformParams const*>(texture.GetPlatformParams());

		// Activate the specified texture unit. Subsequent bind calls will affect this unit
		// TODO: Can this be handled externally to the texture????????
		glActiveTexture(GL_TEXTURE0 + textureUnit);

		if (doBind)
		{
			glBindTexture(params->m_texTarget, params->m_textureID);
			glBindSampler(textureUnit, params->m_samplerID);
		}
		else
		{
			glBindTexture(params->m_texTarget, 0);
			glBindSampler(textureUnit, 0);
		}
	}



	void opengl::Texture::Create(gr::Texture& texture, uint32_t textureUnit)
	{
		LOG("Creating & buffering texture: \"" + texture.GetTexturePath() + "\"");

		// Create the platform-specific parameters object:
		platform::Texture::PlatformParams::CreatePlatformParams(texture);

		// Get our platform params now that the texture has been created:
		PlatformParams* const params =
			dynamic_cast<opengl::Texture::PlatformParams*>(texture.GetPlatformParams());

		// If the texture hasn't been created, create a new name:
		if (!glIsTexture(params->m_textureID))
		{
			glGenTextures(1, &params->m_textureID);

			glBindTexture(params->m_texTarget, params->m_textureID);

			if (glIsTexture(params->m_textureID) != GL_TRUE)
			{
				LOG_ERROR("OpenGL failed to generate new texture name. Texture buffering failed");
				assert("OpenGL failed to generate new texture name. Texture buffering failed" && false);

				glBindTexture(params->m_texTarget, 0);

				return;
			}

			// UV wrap mode:
			glTexParameteri(params->m_texTarget, GL_TEXTURE_WRAP_S, params->m_textureWrapS); // u
			glTexParameteri(params->m_texTarget, GL_TEXTURE_WRAP_T, params->m_textureWrapT); // v
			glTexParameteri(params->m_texTarget, GL_TEXTURE_WRAP_R, params->m_textureWrapR);

			// Mip map min/maximizing:
			glTexParameteri(params->m_texTarget, GL_TEXTURE_MIN_FILTER, params->m_textureMinFilter);
			glTexParameteri(params->m_texTarget, GL_TEXTURE_MAG_FILTER, params->m_textureMaxFilter);
		}
		else
		{
			glBindTexture(params->m_texTarget, params->m_textureID);
		}

		gr::Texture::TextureParams const& texParams = texture.GetTextureParams();
		// Ensure our texture is correctly configured:
		assert(texParams.m_faces == 1 ||
			(texParams.m_faces == 6 && texParams.m_texDimension == gr::Texture::TextureDimension::TextureCubeMap));

		if (texParams.m_texUse == gr::Texture::TextureUse::Color)
		{
			// Configure the Texture sampler:
			glBindSampler(textureUnit, params->m_samplerID);
			if (!glIsSampler(params->m_samplerID))
			{
				glGenSamplers(1, &params->m_samplerID);
				glBindSampler(textureUnit, params->m_samplerID);

				assert("Texture sampler creation failed" && glIsSampler(params->m_samplerID));
			}

			glSamplerParameteri(params->m_samplerID, GL_TEXTURE_WRAP_S, params->m_textureWrapS);
			glSamplerParameteri(params->m_samplerID, GL_TEXTURE_WRAP_T, params->m_textureWrapT);

			glSamplerParameteri(params->m_samplerID, GL_TEXTURE_MIN_FILTER, params->m_textureMinFilter);
			glSamplerParameteri(params->m_samplerID, GL_TEXTURE_MAG_FILTER, params->m_textureMaxFilter);

		}

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
				assert("Color target must have data to buffer" &&
					texture.Texels().size() == (texParams.m_faces * texParams.m_width * texParams.m_height));

				data = (void*)&texture.GetTexel(0, 0, i).r;
			}

			GLenum target;
			if (texParams.m_texDimension == gr::Texture::TextureDimension::TextureCubeMap)
			{
				target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
				// TODO: WHY DO WE USE THIS, BUT SET GL_TEXTURE_CUBE_MAP FOR m_texTarget ELSEWHERE?!?!!?!?!?!?!?!?!?
				// -> Fails if we use GL_TEXTURE_CUBE_MAP...
			}
			else
			{
				target = params->m_texTarget;
			}

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
		if (texture.GetTextureParams().m_useMIPs == false)
		{
			return;
		}

		opengl::Texture::PlatformParams const* const params =
			dynamic_cast<opengl::Texture::PlatformParams*>(texture.GetPlatformParams());

		glGenerateTextureMipmap(params->m_textureID);
	}


	platform::Texture::UVOrigin opengl::Texture::GetUVOrigin()
	{
		return platform::Texture::UVOrigin::BottomLeft;
	}
}