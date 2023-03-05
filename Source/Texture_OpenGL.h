// © 2022 Adam Badke. All rights reserved.
#pragma once

#include <GL/glew.h>

#include "Texture_Platform.h"
#include "Texture.h"


namespace opengl
{
	class Texture
	{
	public:
		struct PlatformParams final : public re::Texture::PlatformParams
		{
			PlatformParams(re::Texture::TextureParams const& texParams);

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
		static void Create(re::Texture& texture);
		static void Destroy(re::Texture& texture);
		static void Bind(re::Texture& texture, uint32_t textureUnit);		
		static void GenerateMipMaps(re::Texture& texture);
	};	
}