// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Window.h"


namespace platform
{
	class Window
	{
	public:
		static void CreatePlatformParams(app::Window&);

	public:
		static bool (*Create)(app::Window& window, std::string const& title, uint32_t width, uint32_t height);
		static void (*Destroy)(app::Window& window);
		static void (*SetRelativeMouseMode)(app::Window const& window, bool enabled);
	};
}