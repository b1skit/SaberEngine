// © 2022 Adam Badke. All rights reserved.
#include "Window_Platform.h"
#include "Window_Win32.h"

#include "Core\Assert.h"


namespace platform
{
	void Window::CreatePlatformParams(app::Window& window)
	{
		window.SetPlatformParams(std::make_unique<win32::Window::PlatformParams>());
	}


	bool (*platform::Window::Create)(app::Window& window, std::string const& title, uint32_t width, uint32_t height) = nullptr;
	void (*platform::Window::Destroy)(app::Window& window) = nullptr;
	void (*platform::Window::SetRelativeMouseMode)(app::Window const& window, bool enabled) = nullptr;
}