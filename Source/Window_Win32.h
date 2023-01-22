// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Window.h"


namespace win32
{
	class Window
	{
	public:
		struct Win32PlatformState
		{
			HINSTANCE m_hInstance = NULL;
		};
		static Win32PlatformState PlatformState;


	public:
		struct PlatformParams final : public virtual en::Window::PlatformParams
		{
			HWND m_hWindow = NULL;
		};


	public:
		static LRESULT CALLBACK WindowEventCallback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);


	public:
		static bool Create(en::Window& window, std::string const& title, uint32_t width, uint32_t height);
		static void Destroy(en::Window& window);
		static void SetRelativeMouseMode(en::Window const& window, bool enabled);
	};
}