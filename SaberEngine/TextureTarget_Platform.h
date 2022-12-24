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


		static void (*CreateColorTargets)(re::TextureTargetSet& targetSet);
		static void (*AttachColorTargets)(re::TextureTargetSet& targetSet, uint32_t face, uint32_t mipLevel);

		static void (*CreateDepthStencilTarget)(re::TextureTargetSet& targetSet);
		static void (*AttachDepthStencilTarget)(re::TextureTargetSet& targetSet);

		static uint32_t(*MaxColorTargets)();
	};
}