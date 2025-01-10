// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Window.h"


namespace platform
{
	class Window
	{
	public:
		static void CreatePlatformParams(host::Window&);

	public:
		static bool (*Create)(host::Window&, std::string const& title, uint32_t width, uint32_t height);
		static void (*Destroy)(host::Window&);
		static void (*SetRelativeMouseMode)(host::Window const&, bool enabled);
	};
}