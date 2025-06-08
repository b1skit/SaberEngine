// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Private/TextureTarget.h"


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
		static void CreatePlatformObject(re::TextureTarget&);
	};


	/******************/
	// TextureTargetSet
	/******************/
	class TextureTargetSet
	{
	public:
		static void CreatePlatformObject(re::TextureTargetSet&);
	};
}