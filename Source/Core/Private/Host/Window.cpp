// © 2022 Adam Badke. All rights reserved.
#include "Private/Window.h"
#include "Private/Window_Platform.h"

#include "Private/../Assert.h"
#include "Private/../EventManager.h"

#include "Private/../Definitions/EventKeys.h"


namespace host
{
	Window::Window()
		: m_hasFocus(false)
		, m_relativeMouseModeEnabled(false)
	{
		platform::Window::CreatePlatformObject(*this);
	}


	Window::~Window()
	{
		SEAssert(!m_platObj, "Window is being destructed with valid platform object. Was Destroy() called?");
	}


	bool Window::Create(CreateParams const& createParams)
	{
		const bool result = platform::Window::Create(*this, createParams);
		SEAssert(result, "Window Create failed");

		platform::Window::SetRelativeMouseMode(*this, m_relativeMouseModeEnabled);

		return result;
	}


	void Window::Destroy()
	{
		platform::Window::Destroy(*this);
		m_platObj = nullptr;
	}


	void Window::SetFocusState(bool hasFocus)
	{
		m_hasFocus = hasFocus;
	
		if (!m_hasFocus)
		{
			platform::Window::SetRelativeMouseMode(*this, false);
		}
		else
		{
			platform::Window::SetRelativeMouseMode(*this, m_relativeMouseModeEnabled);
		}

		core::EventManager::Get()->Notify(core::EventManager::EventInfo{
				.m_eventKey = eventkey::WindowFocusChanged,
				.m_data = m_hasFocus,
			});
	}


	void Window::SetRelativeMouseMode(bool enabled)
	{
		if (enabled != m_relativeMouseModeEnabled)
		{
			platform::Window::SetRelativeMouseMode(*this, enabled);
			m_relativeMouseModeEnabled = enabled;
		}
	}
}