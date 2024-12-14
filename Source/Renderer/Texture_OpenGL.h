// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Texture_Platform.h"
#include "Texture.h"

#include "core/Util/HashUtils.h"

#include <GL/glew.h>


namespace re
{
	struct TextureView;
}

namespace opengl
{
	class Texture
	{
	public:
		struct PlatformParams final : public re::Texture::PlatformParams
		{
			PlatformParams(re::Texture const&);

			~PlatformParams() override;

			void Destroy() override;

			// OpenGL-specific parameters:
			GLuint m_textureID;

			GLenum m_format; // Pixel data format: R, RG, RGBA, etc
			GLenum m_internalFormat; // Number of color components
			GLenum m_type;

			bool m_formatIsImageTextureCompatible;

			mutable std::map<util::DataHash, GLuint> m_textureViews; // OpenGL-equivalent of a descriptor cache
		};


	public:
		// OpenGL-specific functionality:
		static void Create(re::Texture& texture);
		
		static void Bind(re::Texture const&, uint32_t textureUnit);
		static void Bind(re::Texture const&, uint32_t textureUnit, re::TextureView const&);

		static void BindAsImageTexture(
			re::Texture const&, uint32_t textureUnit, re::TextureView const&, uint32_t accessMode);

		static void GenerateMipMaps(re::Texture const&);

		static GLuint GetOrCreateTextureView(re::Texture const&, re::TextureView const&);

		// Platform functionality:
		static void Destroy(re::Texture&);
		static void ShowImGuiWindow(re::Texture const&, float scale);
	};	
}