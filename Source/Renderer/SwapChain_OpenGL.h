// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "SwapChain.h"
#include "TextureTarget.h"


namespace opengl
{
	class SwapChain
	{
	public:
		struct PlatformParams final : public re::SwapChain::PlatformParams
		{
			// OpenGL manages the swap chain implicitly. We just maintain a single target set representing the default
			// framebuffer instead
			std::shared_ptr<re::TextureTargetSet> m_backbufferTargetSet;
		};


	public:
		static void Create(re::SwapChain& swapChain);
		static void Destroy(re::SwapChain& swapChain);
		static bool ToggleVSync(re::SwapChain const& swapChain);

		static std::shared_ptr<re::TextureTargetSet> GetBackBufferTargetSet(re::SwapChain const& swapChain);
	};
}