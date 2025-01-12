// © 2022 Adam Badke. All rights reserved.
#include "Window.h"
#include "Window_Platform.h"

#include "../Assert.h"
#include "../EventManager.h"


namespace host
{
	Window::Window()
		: m_hasFocus(false)
		, m_relativeMouseModeEnabled(false)
	{
		platform::Window::CreatePlatformParams(*this);
	}


	Window::~Window()
	{
		SEAssert(!m_platformParams, "Window is being destructed with valid platform params. Was Destroy() called?");
	}


	bool Window::Create(CreateParams const& createParams)
	{
		return platform::Window::Create(*this, createParams);
	}


	void Window::Destroy()
	{
		platform::Window::Destroy(*this);
		m_platformParams = nullptr;
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
				.m_data0 = m_hasFocus,
				//.m_data1 = unused
			});
	}


	bool Window::GetFocusState() const
	{
		return m_hasFocus;
	}


	void Window::SetRelativeMouseMode(bool enabled)
	{
		if (enabled != m_relativeMouseModeEnabled)
		{
			platform::Window::SetRelativeMouseMode(*this, enabled);
		}
		m_relativeMouseModeEnabled = enabled;
	}
}