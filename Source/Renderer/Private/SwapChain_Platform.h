// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Private/SwapChain.h"
#include "Private/Texture.h"


namespace re
{
	class TextureTargetSet;
}

namespace platform
{
	class SwapChain
	{
	public:
		static void CreatePlatformObject(re::SwapChain&);


	public:
		static void (*Create)(re::SwapChain&, re::Texture::Format);
		static void (*Destroy)(re::SwapChain&);
		static bool (*ToggleVSync)(re::SwapChain const& swapChain);

		// Beware: The backbuffer target set (currently) behaves differently depending on the graphics API.
		// E.g. DX12 has a N TextureTargetSets each with 1 texture resource per frame (i.e. 1 backbuffer resource per
		// frame in flight), while OpenGL has a single empty TextureTargetSet (i.e. no textures) for all frames. Thus it
		// is not possible to arbitrarily get/hold the backbuffer target set in a platform-agnostic way. We primarly
		// provide this accessor as a convenience for debug functionality
		static std::shared_ptr<re::TextureTargetSet>(*GetBackBufferTargetSet)(re::SwapChain const&);
		static re::Texture::Format (*GetBackbufferFormat)(re::SwapChain const&);
		static glm::uvec2 (*GetBackbufferDimensions)(re::SwapChain const&);
	};
}