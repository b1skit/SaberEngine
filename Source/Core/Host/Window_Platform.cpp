// © 2022 Adam Badke. All rights reserved.
#include "Window_Platform.h"
#include "Window_Win32.h"

#include "../Assert.h"


namespace platform
{
	void Window::CreatePlatformParams(host::Window& window)
	{
		window.SetPlatformParams(std::make_unique<win32::Window::PlatformParams>());
	}


	bool (*platform::Window::Create)(host::Window&, std::string const& title, uint32_t width, uint32_t height) = nullptr;
	void (*platform::Window::Destroy)(host::Window&) = nullptr;
	void (*platform::Window::SetRelativeMouseMode)(host::Window const& window, bool enabled) = nullptr;
}