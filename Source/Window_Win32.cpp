// © 2022 Adam Badke. All rights reserved.
#include <GL/glew.h>
#include <GL/GL.h> // Must follow glew.h

#include "Window.h"
#include "Window_Win32.h"
#include "DebugConfiguration.h"
#include "CoreEngine.h"


namespace win32
{
	win32::Window::Win32PlatformState win32::Window::PlatformState;


	LRESULT CALLBACK Window::WindowEventCallback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		LRESULT result = 0;

		switch (msg)
		{
		case WM_CLOSE:
		case WM_DESTROY:
		case WM_QUIT:
		{
			::PostQuitMessage(0);
		}
		break;
		case WM_SETFOCUS:
		{
			en::CoreEngine::Get()->GetWindow()->SetFocusState(true);
		}
		break;
		case WM_KILLFOCUS:
		{
			en::CoreEngine::Get()->GetWindow()->SetFocusState(false);
		}
		break;
		default:
			result = DefWindowProcW(window, msg, wParam, lParam);
			break;
		}

		return result;
	}

	
	bool Window::Create(re::Window& window, std::string const& title, uint32_t width, uint32_t height)
	{
		// Since the Windows 10 Creators update, we have per-monitor V2 DPI awareness context. This allows the client
		// area of the window to achieve 100% scaling while still allowing non-client window content to be rendered in
		// a DPI-sensitive fashion
		SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

		// Window class name. Used for registering / creating the window.
		const wchar_t* const windowClassName = L"SaberEngineWindow"; // Unique window identifier

		// Register a window class for creating our render window with
		WNDCLASSEXW windowClass = {};

		windowClass.cbSize = sizeof(WNDCLASSEX); // Size of the structure
		windowClass.style = CS_HREDRAW | CS_VREDRAW; // CS_HREDRAW/CS_VREDRAW: Redraw entire window if movement/size adjustment changes the window width/height
		windowClass.lpfnWndProc = (WNDPROC)win32::Window::WindowEventCallback; // Window message handler function pointer
		windowClass.cbClsExtra = 0; // # of extra bytes following the structure. 0, as not used here
		windowClass.cbWndExtra = 0; // # of extra bytes to allocate following the structure. 0, as not used here
		windowClass.hInstance = win32::Window::PlatformState.m_hInstance; // Handle to the instance containing the window procedure
		windowClass.hIcon = ::LoadIcon(win32::Window::PlatformState.m_hInstance, NULL); // Handle to class icon that represents this class in the taskbar, and upper-left corner of the title bar. Null = default
		windowClass.hCursor = ::LoadCursor(NULL, IDC_ARROW); // Handle to the class cursor. IDC_ARROW = default arrow icon
		windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Handle to the class background brush. COLOR_WINDOW + 1 = COLOR_WINDOWFRAME
		windowClass.lpszMenuName = NULL; // Null-terminated char string for the resource name of the class menu (as it appears in the resource file)
		windowClass.lpszClassName = windowClassName; // Set the unique window identifier
		windowClass.hIconSm = ::LoadIcon(win32::Window::PlatformState.m_hInstance, NULL); // Handle to the small icon associated with the window class. Null = Search the windowClass.hIcon resource

		if (!::RegisterClassExW(&windowClass))
		{
			SEAssertF("Failed to register window");
			return false;
		}

		// Get the width/height of the primary display
		int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
		int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

		// Calculate the coordinates of the top-left/bottom-right corners of the desired client area:
		RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
		::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
		// WS_OVERLAPPEDWINDOW: Can be min/maximized, has a thick window frame

		// Compute the width/height of the window we're creating:
		int windowWidth = windowRect.right - windowRect.left;
		int windowHeight = windowRect.bottom - windowRect.top;
		// Note: We can't use the received width/height directly, as it may result in a window that's larger than the viewable area

		// Center the window within the screen. Clamp to 0, 0 for the top-left corner
		int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
		int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

		win32::Window::PlatformParams* const platformParams =
			dynamic_cast<win32::Window::PlatformParams*>(window.GetPlatformParams());

		std::wstring titleWideStr = std::wstring(title.begin(), title.end());

		platformParams->m_hWindow = ::CreateWindowExW(
			NULL, // Extended window styles: https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles
			windowClass.lpszClassName, // Unique window class name
			titleWideStr.c_str(), // Window/titlebar name
			WS_OVERLAPPEDWINDOW, // Window styles: https://learn.microsoft.com/en-us/windows/win32/winmsg/window-styles
			windowX, // Initial horizontal position
			windowY, // Initial vertical position
			windowWidth, // Window width
			windowHeight, // Window height
			NULL, // Optional: Handle to the window parent
			NULL, // Handle to a menu, or, specifies a child-window identifier
			win32::Window::PlatformState.m_hInstance, // Handle to the instance of the module associated with the window
			nullptr // Pointer to a value that will be passed to the window through the CREATESTRUCT. Sent by this function before it returns
		);

		SEAssert("Failed to create window", platformParams->m_hWindow);

		::ShowWindow(platformParams->m_hWindow, SW_SHOW);
		::UpdateWindow(platformParams->m_hWindow);

		return true;
	}


	void Window::Destroy(re::Window& window)
	{
		win32::Window::PlatformParams* const platformParams =
			dynamic_cast<win32::Window::PlatformParams*>(window.GetPlatformParams());

		::DestroyWindow(platformParams->m_hWindow);
	}


	void Window::SetRelativeMouseMode(re::Window const& window, bool enabled)
	{
		if (enabled)
		{
			win32::Window::PlatformParams* const platformParams =
				dynamic_cast<win32::Window::PlatformParams*>(window.GetPlatformParams());

			// Wrap mouse movements about the screen rectangle:
			RECT rect;
			::GetClientRect(platformParams->m_hWindow, &rect);

			POINT upperLeft;
			upperLeft.x = rect.left;
			upperLeft.y = rect.top;
			::MapWindowPoints(platformParams->m_hWindow, nullptr, &upperLeft, 1);

			POINT lowerRight;
			lowerRight.x = rect.right;
			lowerRight.y = rect.bottom;
			::MapWindowPoints(platformParams->m_hWindow, nullptr, &lowerRight, 1);

			rect.left = upperLeft.x;
			rect.top = upperLeft.y;
			rect.right = lowerRight.x;
			rect.bottom = lowerRight.y;

			::ClipCursor(&rect);
			::ShowCursor(false); // Hide the cursor
		}
		else
		{
			::ClipCursor(0);
			::ShowCursor(true);
		}
	}
}