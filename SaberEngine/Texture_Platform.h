#pragma once

#include <memory>

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


		static void CreatePlatformParams(gr::Texture& texture);


		// API-specific function bindings:
		/*********************************/
		static void (*Create)(gr::Texture&);
		static void (*Bind)(gr::Texture&, uint32_t textureUnit, bool doBind);
		static void (*Destroy)(gr::Texture&);

		static void (*GenerateMipMaps)(gr::Texture&);
		static UVOrigin (*GetUVOrigin)();
	};
}

