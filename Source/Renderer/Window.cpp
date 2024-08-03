// © 2022 Adam Badke. All rights reserved.
#include "Window.h"
#include "Window_Platform.h"

#include "Core/Assert.h"
#include "Core/EventManager.h"


namespace app
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


	bool Window::InitializeFromEventQueueThread(std::string const& title, uint32_t width, uint32_t height)
	{
		// Must be called from the thread that owns the OS event queue
		return platform::Window::Create(*this, title, width, height);
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
				.m_type = core::EventManager::EventType::WindowFocusChanged,
				.m_data0 = core::EventManager::EventData{.m_dataB = m_hasFocus}
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