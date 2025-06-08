// © 2022 Adam Badke. All rights reserved.
#include "SwapChain.h"
#include "SwapChain_Platform.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/EventManager.h"

#include "Core/Definitions/EventKeys.h"


namespace re
{
	SwapChain::SwapChain()
		: m_platObj(nullptr)
	{
		platform::SwapChain::CreatePlatformObject(*this);
	}


	SwapChain::~SwapChain()
	{
		SEAssert(m_platObj == nullptr, "~SwapChain() called before Destroy()");
	}


	void SwapChain::Create(re::Texture::Format format)
	{
		m_platObj->m_vsyncEnabled = core::Config::Get()->GetValue<bool>(core::configkeys::k_vsyncEnabledKey);

		platform::SwapChain::Create(*this, format);

		// Broadcast the starting VSync state:
		core::EventManager::Get()->Notify(core::EventManager::EventInfo{
			.m_eventKey = eventkey::VSyncModeChanged,
			.m_data = m_platObj->m_vsyncEnabled, });
	}


	void SwapChain::Destroy()
	{
		platform::SwapChain::Destroy(*this);
		m_platObj = nullptr;
	}


	bool SwapChain::GetVSyncState() const
	{
		return m_platObj->m_vsyncEnabled;
	}


	bool SwapChain::ToggleVSync() const
	{
		const bool vsyncState = platform::SwapChain::ToggleVSync(*this);

		core::EventManager::Get()->Notify(core::EventManager::EventInfo{
					.m_eventKey = eventkey::VSyncModeChanged,
					.m_data = vsyncState, });

		return vsyncState;
	}
}
