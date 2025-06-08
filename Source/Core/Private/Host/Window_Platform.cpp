// © 2022 Adam Badke. All rights reserved.
#include "Window_Platform.h"
#include "Window_Win32.h"


namespace platform
{
	void Window::CreatePlatformObject(host::Window& window)
	{
		window.SetPlatformObject(std::make_unique<win32::Window::PlatObj>());
	}


	bool (*platform::Window::Create)(host::Window&, host::Window::CreateParams const&) = nullptr;
	void (*platform::Window::Destroy)(host::Window&) = nullptr;
	void (*platform::Window::SetRelativeMouseMode)(host::Window const& window, bool enabled) = nullptr;
}