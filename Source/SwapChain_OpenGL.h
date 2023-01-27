// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "SwapChain.h"
#include "TextureTarget.h"


namespace opengl
{
	class SwapChain
	{
	public:
		struct PlatformParams final : public virtual re::SwapChain::PlatformParams
		{
			// OpenGL manages the swap chain implicitly. We just maintain a target set representing the default
			// framebuffer instead
			// Note: We store this as a shared_ptr so we can instantiate it once the context has been initialized
			std::shared_ptr<re::TextureTargetSet> m_backbuffer;
		};


	public:
		static void Create(re::SwapChain& swapChain);
		static void Destroy(re::SwapChain& swapChain);
		static void SetVSyncMode(re::SwapChain const& swapChain, bool enabled);
	};
}