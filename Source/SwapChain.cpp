// © 2022 Adam Badke. All rights reserved.
#include "SwapChain.h"
#include "SwapChain_Platform.h"


namespace re
{
	SwapChain::SwapChain()
		: m_platformParams(nullptr)
	{
		platform::SwapChain::CreatePlatformParams(*this);
	}


	void SwapChain::Create()
	{
		platform::SwapChain::Create(*this);
	}


	void SwapChain::Destroy()
	{
		platform::SwapChain::Destroy(*this);
	}
}
