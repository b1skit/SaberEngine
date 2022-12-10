#pragma once

#include <memory>

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
		static void (*AttachColorTargets)(re::TextureTargetSet& targetSet, uint32_t face, uint32_t mipLevel, bool doBind);

		static void (*CreateDepthStencilTarget)(re::TextureTargetSet& targetSet);
		static void (*AttachDepthStencilTarget)(re::TextureTargetSet& targetSet, bool doBind);

		static uint32_t(*MaxColorTargets)();
	};
}