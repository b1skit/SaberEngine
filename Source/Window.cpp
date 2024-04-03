// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "EventManager.h"
#include "Window.h"
#include "Window_Platform.h"

namespace en
{
	Window::Window()
		: m_hasFocus(false)
		, m_relativeMouseModeEnabled(false)
	{
		platform::Window::CreatePlatformParams(*this);
	}


	bool Window::Create(std::string const& title, uint32_t width, uint32_t height)
	{
		return platform::Window::Create(*this, title, width, height);
	}


	void Window::Destroy()
	{
		platform::Window::Destroy(*this);
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

		en::EventManager::Get()->Notify(en::EventManager::EventInfo{
				.m_type = en::EventManager::EventType::WindowFocusChanged,
				.m_data0 = en::EventManager::EventData{.m_dataB = m_hasFocus}
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