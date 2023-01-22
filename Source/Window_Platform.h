// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Window.h"


namespace platform
{
	class Window
	{
	public:
		static void CreatePlatformParams(en::Window&);

	public:
		static bool (*Create)(en::Window& window, std::string const& title, uint32_t width, uint32_t height);
		static void (*Destroy)(en::Window& window);
		static void (*SetRelativeMouseMode)(en::Window const& window, bool enabled);
	};
}