// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "SwapChain.h"


namespace dx12
{
	class SwapChain
	{
	public:
		struct PlatformParams final : public virtual re::SwapChain::PlatformParams
		{

		};


	public:
		static void Create(re::SwapChain& swapChain);
		static void Destroy(re::SwapChain& swapChain);
	};
}