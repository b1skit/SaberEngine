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
		static void (*Destroy)(re::Texture&);
	};
}

