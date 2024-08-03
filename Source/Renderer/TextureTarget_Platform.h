// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "TextureTarget.h"


namespace re
{
	class Texture;
}

namespace platform
{
	/***************/
	// TextureTarget
	/***************/
	class TextureTarget
	{
	public:
		static void CreatePlatformParams(re::TextureTarget&);
	};


	/******************/
	// TextureTargetSet
	/******************/
	class TextureTargetSet
	{
	public:
		static void CreatePlatformParams(re::TextureTargetSet&);
	};
}