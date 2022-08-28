#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "Texture_Platform.h"

// Predeclarations
namespace gr
{
	class Texture;
}


namespace opengl
{
	class Texture
	{
	public:
		struct PlatformParams : public virtual platform::Texture::PlatformParams
		{
			PlatformParams(gr::Texture::TextureParams const& texParams);

			~PlatformParams() override;

			// OpenGL-specific parameters:
			GLuint m_textureID;

			GLenum m_texTarget;

			// NOTE: Currently, SaberEngine assumes all textures contain 4-channel vec4's (except for depth).
			// If format != GL_RGBA, buffer will be packed with the wrong stride
			GLenum m_format;

			GLenum m_internalFormat;
			GLenum m_type;

			glm::vec4 m_clearColor;
		};


	public:
		static void Create(gr::Texture& texture, uint32_t textureUnit);
		static void Bind(gr::Texture const& mesh, uint32_t textureUnit, bool doBind = true);
		static void Destroy(gr::Texture& texture);
		static void GenerateMipMaps(gr::Texture& texture);

		static platform::Texture::UVOrigin GetUVOrigin();
	};	
}