// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Texture.h"

#include "Core/Util/HashKey.h"

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
			PlatformParams(re::Texture&);

			~PlatformParams() override;

			void Destroy() override;

			// OpenGL-specific parameters:
			GLuint m_textureID;

			GLenum m_format; // Pixel data format: R, RG, RGBA, etc
			GLenum m_internalFormat; // Number of color components
			GLenum m_type;

			bool m_formatIsImageTextureCompatible;

			mutable std::map<util::HashKey, GLuint> m_textureViews; // OpenGL-equivalent of a descriptor cache
		};


	public:
		// OpenGL-specific functionality:
		static void Create(core::InvPtr<re::Texture> const& texture, void* unused);
		
		static void Bind(core::InvPtr<re::Texture> const&, uint32_t textureUnit);
		static void Bind(core::InvPtr<re::Texture> const&, uint32_t textureUnit, re::TextureView const&);

		static void BindAsImageTexture(
			core::InvPtr<re::Texture> const&, uint32_t textureUnit, re::TextureView const&, uint32_t accessMode);

		static void GenerateMipMaps(core::InvPtr<re::Texture> const&);

		static GLuint GetOrCreateTextureView(core::InvPtr<re::Texture> const&, re::TextureView const&);

		// Platform functionality:
		static void Destroy(re::Texture&);
		static void ShowImGuiWindow(core::InvPtr<re::Texture> const&, float scale);
	};	
}