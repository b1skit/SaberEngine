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

			GLenum m_format; // Pixel data format: R, RG, RGBA, etc
			GLenum m_internalFormat; // Number of color components
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