// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Texture.h"


namespace platform
{
	class Texture
	{
	public:
		static void CreatePlatformParams(re::Texture& texture);


		// API-specific function bindings:
		/*********************************/
		static void (*Create)(re::Texture&);
		static void (*Destroy)(re::Texture&);
		static void (*Bind)(re::Texture&, uint32_t textureUnit);
		static void (*GenerateMipMaps)(re::Texture&);
	};
}

