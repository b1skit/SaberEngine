// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "SwapChain.h"

namespace platform
{
	class SwapChain
	{
	public:
		static void CreatePlatformParams(re::SwapChain&);


	public:
		static void (*Create)(re::SwapChain&);
		static void (*Destroy)(re::SwapChain&);
	};
}