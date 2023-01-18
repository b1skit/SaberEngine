// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Window.h"


namespace platform
{
	class Window
	{
	public:
		static void CreatePlatformParams(re::Window&);

	public:
		static bool (*Create)(re::Window& window, std::string const& title, uint32_t width, uint32_t height);
		static void (*Destroy)(re::Window& window);
		static void (*SetRelativeMouseMode)(re::Window const& window, bool enabled);
	};
}