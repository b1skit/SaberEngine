#pragma once

#include "Texture.h"


namespace platform
{
	class Texture
	{
	public:
		enum class UVOrigin
		{
			BottomLeft,	// OpenGL style
			TopLeft,	// D3D style

			Invalid,
			PlatformTextureUVOriginCount = Invalid
		};


		static void CreatePlatformParams(re::Texture& texture);


		// API-specific function bindings:
		/*********************************/
		static void (*Create)(re::Texture&);
		static void (*Bind)(re::Texture&, uint32_t textureUnit);
		static void (*Destroy)(re::Texture&);

		static void (*GenerateMipMaps)(re::Texture&);
		static UVOrigin (*GetUVOrigin)();
	};
}

