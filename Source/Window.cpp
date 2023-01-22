// © 2022 Adam Badke. All rights reserved.
#include "Window.h"
#include "Window_Platform.h"

namespace en
{
	Window::Window()
		: m_hasFocus(false)
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
	}


	bool Window::GetFocusState() const
	{
		return m_hasFocus;
	}


	void Window::SetRelativeMouseMode(bool enabled) const
	{
		platform::Window::SetRelativeMouseMode(*this, enabled);
	}
}