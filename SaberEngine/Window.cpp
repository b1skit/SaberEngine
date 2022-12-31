#include "Window.h"

namespace re
{
	Window::Window()
	{
		platform::Window::PlatformParams::CreatePlatformParams(*this);
	}


	bool Window::Create(std::string const& title, uint32_t width, uint32_t height)
	{
		return platform::Window::Create(*this, title, width, height);
	}


	void Window::Destroy()
	{
		platform::Window::Destroy(*this);
	}


	void Window::Present() const
	{
		platform::Window::Present(*this);
	}


	bool Window::HasFocus() const
	{
		return platform::Window::HasFocus(*this);
	}
}