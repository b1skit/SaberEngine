// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Window.h"


namespace platform
{
	class Window
	{
	public:
		static void CreatePlatformObject(host::Window&);

	public:
		static bool (*Create)(host::Window&, host::Window::CreateParams const&);
		static void (*Destroy)(host::Window&);
		static void (*SetRelativeMouseMode)(host::Window const&, bool enabled);
	};
}