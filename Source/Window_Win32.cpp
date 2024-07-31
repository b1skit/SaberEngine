// © 2022 Adam Badke. All rights reserved.
#include "Context.h"
#include "Window.h"
#include "Window_Win32.h"

#include "Core/Assert.h"
#include "Core/EventManager.h"


namespace win32
{
	win32::Window::Win32PlatformState win32::Window::s_platformState;

	
	// Handle messages we've (re)broadcasted (i.e. tranlated & dispatched) from win32::EventManager::ProcessMessages
	LRESULT CALLBACK Window::WindowEventCallback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		core::EventManager::EventInfo eventInfo;
		bool doBroadcastSEEvent = true;

		LRESULT result = 0;

		switch (uMsg)
		{
		case WM_CLOSE:
		case WM_DESTROY:
		case WM_QUIT:
		{
			eventInfo.m_type = core::EventManager::EventType::EngineQuit;

			::PostQuitMessage(0);
		}
		break;
		case WM_SYSCOMMAND:
		{
			// Maximize/minimize/restore/close buttons, or a command from the Window menu
			// https://learn.microsoft.com/en-us/windows/win32/menurc/wm-syscommand
			if (wParam == SC_CLOSE)
			{
				eventInfo.m_type = core::EventManager::EventType::EngineQuit;
			}
			else
			{
				doBroadcastSEEvent = false;
			}
		}
		break;
		case WM_SYSCHAR:
		{
			// WM_SYSCHAR is posted when TranslateMessage is called on a WM_SYSKEYDOWN message. The default window 
			// procedure plays a system notification sound when pressing Alt+Enter if this message is not handled
			doBroadcastSEEvent = false;
		}
		break;
		case WM_SETFOCUS:
		case WM_EXITSIZEMOVE:
		{
			re::Context::Get()->GetWindow()->SetFocusState(true);
			doBroadcastSEEvent = false;
		}
		break;
		case WM_KILLFOCUS:
		case WM_ENTERSIZEMOVE:
		{
			re::Context::Get()->GetWindow()->SetFocusState(false);
			doBroadcastSEEvent = false;
		}
		break;
		case WM_SYSKEYDOWN: // ALT + any key (aka a "system keypress"), or F10 (actives menu)
		case WM_KEYDOWN: // Non-system keypress (i.e. "normal" keypresses)
		case WM_SYSKEYUP:
		case WM_KEYUP:
		{
			eventInfo.m_type = core::EventManager::EventType::KeyEvent;

			switch (wParam)
			{
			case VK_CONTROL:
			case VK_SHIFT:
			case VK_MENU: //alt
			{
				// Determine whether the left/right instance of control/shift/alt was pressed
				const WORD keyFlags = HIWORD(lParam);
				WORD scanCode = LOBYTE(keyFlags);
				const bool isExtendedKey = (keyFlags & KF_EXTENDED) == KF_EXTENDED;
				if (isExtendedKey)
				{
					scanCode = MAKEWORD(scanCode, 0xE0);
				}
				WORD vkCode = vkCode = LOWORD(MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX)); // virtual-key code

				// Pack VK_LSHIFT/VK_RSHIFT/VK_LCONTROL/VK_RCONTROL/VK_LMENU/VK_RMENU
				eventInfo.m_data0.m_dataUI = static_cast<uint32_t>(vkCode);
			}
			break;
			default: // Regular key press
			{
				eventInfo.m_data0.m_dataUI = static_cast<uint32_t>(wParam); // Win32 virtual key code
			}
			}

			// Key is down if the most significant bit is set
			constexpr unsigned short k_mostSignificantBit = 1 << (std::numeric_limits<short>::digits);

			// true/false == pressed/released
			eventInfo.m_data1.m_dataB =
				static_cast<bool>(GetAsyncKeyState(static_cast<int>(wParam)) & k_mostSignificantBit);
		}
		break;
		case WM_CHAR:
		{
			// Posted when a WM_KEYDOWN message is translated by TranslateMessage
			eventInfo.m_type = core::EventManager::EventType::TextInputEvent;
			eventInfo.m_data0.m_dataC = static_cast<char>(wParam);
		}
		break;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		{
			eventInfo.m_type = core::EventManager::EventType::MouseButtonEvent;
			eventInfo.m_data0.m_dataUI = 0;
			eventInfo.m_data1.m_dataB = (uMsg == WM_LBUTTONDOWN);
		}
		break;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		{
			eventInfo.m_type = core::EventManager::EventType::MouseButtonEvent;
			eventInfo.m_data0.m_dataUI = 1;
			eventInfo.m_data1.m_dataB = (uMsg == WM_MBUTTONDOWN);
		}
		break;
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		{
			eventInfo.m_type = core::EventManager::EventType::MouseButtonEvent;
			eventInfo.m_data0.m_dataUI = 2;
			eventInfo.m_data1.m_dataB = (uMsg == WM_RBUTTONDOWN);
		}
		break;
		case WM_MOUSEWHEEL:
		{
			eventInfo.m_type = core::EventManager::EventType::MouseWheelEvent;
			eventInfo.m_data0.m_dataI = 0; // X: Currently not supported
			eventInfo.m_data1.m_dataI = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA; // Y
			// Note: Wheel motion is in of +/- units WHEEL_DELTA == 120
		}
		break;
		case WM_INPUT:
		{
			UINT dwSize = sizeof(RAWINPUT);
			static BYTE lpb[sizeof(RAWINPUT)];

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));

			RAWINPUT* raw = (RAWINPUT*)lpb;

			if (raw->header.dwType == RIM_TYPEMOUSE)
			{
				eventInfo.m_type = core::EventManager::EventType::MouseMotionEvent;

				eventInfo.m_data0.m_dataI = raw->data.mouse.lLastX;
				eventInfo.m_data1.m_dataI = raw->data.mouse.lLastY;
			}
			else
			{
				doBroadcastSEEvent = false;
			}
		}
		break;
		default:
			result = DefWindowProcW(hWnd, uMsg, wParam, lParam);
			doBroadcastSEEvent = false;
		}


		if (doBroadcastSEEvent)
		{
			core::EventManager::Get()->Notify(std::move(eventInfo));
		}

		return result;
	}

	
	bool Window::Create(app::Window& window, std::string const& title, uint32_t width, uint32_t height)
	{
		// Since the Windows 10 Creators update, we have per-monitor V2 DPI awareness context. This allows the client
		// area of the window to achieve 100% scaling while still allowing non-client window content to be rendered in
		// a DPI-sensitive fashion
		SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

		// Window class name. Used for registering / creating the window.
		wchar_t const* const windowClassName = L"SaberEngineWindow"; // Unique window identifier

		// Cache the standard cursor:
		win32::Window::s_platformState.m_defaultCursor = ::LoadCursor(NULL, IDC_ARROW); // IDC_ARROW = default arrow icon

		// Register a window class for creating our render window with
		const WNDCLASSEXW windowClass = {
			.cbSize = sizeof(WNDCLASSEX), // Size of the structure
			.style = CS_HREDRAW | CS_VREDRAW, // CS_HREDRAW/CS_VREDRAW: Redraw entire window if movement/size adjustment changes the window width/height
			.lpfnWndProc = (WNDPROC)win32::Window::WindowEventCallback, // Window message handler function pointer
			.cbClsExtra = 0, // # of extra bytes following the structure. 0, as not used here
			.cbWndExtra = 0, // # of extra bytes to allocate following the structure. 0, as not used here
			.hInstance = win32::Window::s_platformState.m_hInstance, // Handle to the instance containing the window procedure
			.hIcon = ::LoadIcon(win32::Window::s_platformState.m_hInstance, 0), // Handle to class icon that represents this class in the taskbar, and upper-left corner of the title bar. Null = default
			.hCursor = NULL, // Class cursor handle: NULL prevents cursor being restored every time the mouse moves
			.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1), // Handle to the class background brush. COLOR_WINDOW + 1 = COLOR_WINDOWFRAME
			.lpszMenuName = NULL, // Null-terminated char string for the resource name of the class menu (as it appears in the resource file)
			.lpszClassName = windowClassName, // Set the unique window identifier
			.hIconSm = ::LoadIcon(win32::Window::s_platformState.m_hInstance, NULL) }; // Handle to the small icon associated with the window class. Null = Search the windowClass.hIcon resource

		if (!::RegisterClassExW(&windowClass))
		{
			SEAssertF("Failed to register hWnd");
			return false;
		}

		// Get the width/height of the primary display
		const int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
		const int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

		constexpr uint32_t k_windowStyle = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;

		// Calculate the coordinates of the top-left/bottom-right corners of the desired client area:
		RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
		::AdjustWindowRect(&windowRect, k_windowStyle, FALSE);
		// WS_OVERLAPPEDWINDOW: Can be min/maximized, has a thick window frame

		// Compute the width/height of the window we're creating:
		const int windowWidth = windowRect.right - windowRect.left;
		const int windowHeight = windowRect.bottom - windowRect.top;
		// Note: We can't use the received width/height directly, as it may result in a window that's larger than the viewable area

		// Center the window within the screen. Clamp to 0, 0 for the top-left corner
		const int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
		const int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

		win32::Window::PlatformParams* platformParams = window.GetPlatformParams()->As<win32::Window::PlatformParams*>();

		const std::wstring titleWideStr = std::wstring(title.begin(), title.end());

		platformParams->m_hWindow = ::CreateWindowExW(
			NULL, // Extended window styles: https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles
			windowClass.lpszClassName, // Unique window class name
			titleWideStr.c_str(), // Window/titlebar name
			k_windowStyle, // Window styles: https://learn.microsoft.com/en-us/windows/win32/winmsg/window-styles
			windowX, // Initial horizontal position
			windowY, // Initial vertical position
			windowWidth, // Window width
			windowHeight, // Window height
			NULL, // Optional: Handle to the window parent
			NULL, // Handle to a menu, or, specifies a child-window identifier
			win32::Window::s_platformState.m_hInstance, // Handle to the instance of the module associated with the window
			nullptr // Pointer to a value that will be passed to the window through the CREATESTRUCT. Sent by this function before it returns
		);

		SEAssert(platformParams->m_hWindow, "Failed to create hWnd");

		::ShowWindow(platformParams->m_hWindow, SW_SHOW);
		::UpdateWindow(platformParams->m_hWindow);

		// Register the mouse as a raw input device:
		// https://learn.microsoft.com/en-us/windows/win32/dxtecharts/taking-advantage-of-high-dpi-mouse-movement?redirectedfrom=MSDN
		{
			RAWINPUTDEVICE rawInputDevice[1];
			rawInputDevice[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
			rawInputDevice[0].usUsage = HID_USAGE_GENERIC_MOUSE;
			rawInputDevice[0].dwFlags = RIDEV_INPUTSINK;

			rawInputDevice[0].hwndTarget = platformParams->m_hWindow;
			RegisterRawInputDevices(rawInputDevice, 1, sizeof(rawInputDevice[0]));
		}

		return true;
	}


	void Window::Destroy(app::Window& window)
	{
		win32::Window::PlatformParams* platformParams = 
			window.GetPlatformParams()->As<win32::Window::PlatformParams*>();

		::DestroyWindow(platformParams->m_hWindow);
	}


	void Window::SetRelativeMouseMode(app::Window const& window, bool relativeModeEnabled)
	{
		if (relativeModeEnabled)
		{
			win32::Window::PlatformParams* platformParams = 
				window.GetPlatformParams()->As<win32::Window::PlatformParams*>();

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

			::SetCursor(NULL); // Hide the cursor
		}
		else
		{
			::ClipCursor(nullptr);

			::SetCursor(win32::Window::s_platformState.m_defaultCursor); // Restore the cursor
		}
	}
}