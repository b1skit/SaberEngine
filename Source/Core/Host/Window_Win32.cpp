// © 2022 Adam Badke. All rights reserved.
#include "Window.h"
#include "Window_Win32.h"

#include "../Assert.h"
#include "../EventManager.h"
#include "../Logger.h"

#include "../Util/CastUtils.h"

#include "shellapi.h"
#include "shellscalingapi.h"
#include "winuser.h"


namespace win32
{
	win32::Window::Win32PlatformState win32::Window::s_platformState{};

	
	// Handle messages we've (re)broadcasted (i.e. tranlated & dispatched) from win32::EventManager::ProcessMessages
	LRESULT CALLBACK Window::WindowEventCallback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		core::EventManager::EventInfo eventInfo;
		bool doBroadcastSEEvent = true;
		bool doRebroadcastWinEvent = true;

		LRESULT result = 0;

		host::Window* window = reinterpret_cast<host::Window*>(::GetWindowLongPtrA(hWnd, GWLP_USERDATA));

		switch (uMsg)
		{
		case WM_CLOSE:
		case WM_DESTROY:
		case WM_QUIT:
		{
			eventInfo.m_eventKey = eventkey::EngineQuit;

			::PostQuitMessage(0);
		}
		break;
		case WM_SYSCOMMAND:
		{
			// Maximize/minimize/restore/close buttons, or a command from the Window menu
			// https://learn.microsoft.com/en-us/windows/win32/menurc/wm-syscommand
			if (wParam == SC_CLOSE)
			{
				eventInfo.m_eventKey = eventkey::EngineQuit;
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
			window->SetFocusState(true);
			doBroadcastSEEvent = false;
		}
		break;
		case WM_KILLFOCUS:
		case WM_ENTERSIZEMOVE:
		{
			window->SetFocusState(false);
			doBroadcastSEEvent = false;
		}
		break;
		case WM_SYSKEYDOWN: // ALT + any key (aka a "system keypress"), or F10 (actives menu)
		case WM_KEYDOWN: // Non-system keypress (i.e. "normal" keypresses)
		case WM_SYSKEYUP:
		case WM_KEYUP:
		{
			eventInfo.m_eventKey = eventkey::KeyEvent;
			eventInfo.m_data = std::pair<uint32_t, bool>(0, 0);
			std::pair<uint32_t, bool>& dataRef = std::get<std::pair<uint32_t, bool>>(eventInfo.m_data);

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
				dataRef.first = static_cast<uint32_t>(vkCode);
			}
			break;
			default: // Regular key press
			{
				dataRef.first = static_cast<uint32_t>(wParam); // Win32 virtual key code
			}
			}

			// Key is down if the most significant bit is set
			constexpr unsigned short k_mostSignificantBit = 1 << (std::numeric_limits<short>::digits);

			// true/false == pressed/released
			dataRef.second =
				static_cast<bool>(GetAsyncKeyState(static_cast<int>(wParam)) & k_mostSignificantBit);
		}
		break;
		case WM_CHAR:
		{
			// Posted when a WM_KEYDOWN message is translated by TranslateMessage
			eventInfo.m_eventKey = eventkey::TextInputEvent;
			eventInfo.m_data = static_cast<char>(wParam);
		}
		break;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		{
			eventInfo.m_eventKey = eventkey::MouseButtonEvent;
			eventInfo.m_data = std::pair<uint32_t, bool>(
				0u,
				uMsg == WM_LBUTTONDOWN);
		}
		break;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		{
			eventInfo.m_eventKey = eventkey::MouseButtonEvent;
			eventInfo.m_data = std::pair<uint32_t, bool>(
				1u,
				uMsg == WM_MBUTTONDOWN);
		}
		break;
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		{
			eventInfo.m_eventKey = eventkey::MouseButtonEvent;
			eventInfo.m_data = std::pair<uint32_t, bool>(
				2u,
				uMsg == WM_RBUTTONDOWN);
		}
		break;
		case WM_MOUSEWHEEL:
		{
			eventInfo.m_eventKey = eventkey::MouseWheelEvent;
			eventInfo.m_data = std::pair<int32_t, int32_t>(
				0,																	// X: Currently not supported
				static_cast<int>(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA));	// Y
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
				eventInfo.m_eventKey = eventkey::MouseMotionEvent;

				eventInfo.m_data = std::pair<int32_t, int32_t>(
					static_cast<int>(raw->data.mouse.lLastX),
					static_cast<int>(raw->data.mouse.lLastY));
			}
			else
			{
				doBroadcastSEEvent = false;
			}
		}
		break;
		case WM_NCCREATE:
		{
			// Window creation: Retrieve our host::Window* and store it in the win32 Window's user data:
			CREATESTRUCTA* createStruct = reinterpret_cast<CREATESTRUCTA*>(lParam);
		
			::SetWindowLongPtrA(
				hWnd,
				GWLP_USERDATA,
				reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));

			doBroadcastSEEvent = false;
		}
		break;
		default:
			doRebroadcastWinEvent = true;
			doBroadcastSEEvent = false;
		}

		if (doBroadcastSEEvent)
		{
			core::EventManager::Get()->Notify(std::move(eventInfo));
		}

		if (doRebroadcastWinEvent)
		{
			result = DefWindowProcW(hWnd, uMsg, wParam, lParam);
		}

		return result;
	}

	
	bool Window::Create(host::Window& window, host::Window::CreateParams const& createParams)
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

		// WS_OVERLAPPEDWINDOW: Can be min/maximized, has a thick window frame
		constexpr uint32_t k_windowStyle = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;

		// Calculate the coordinates of the top-left/bottom-right corners of the desired client area:
		RECT windowRect = {
			0,
			0, 
			util::CheckedCast<LONG>(createParams.m_width), 
			util::CheckedCast<LONG>(createParams.m_height) };
		::AdjustWindowRect(&windowRect, k_windowStyle, FALSE);
		
		// Compute the width/height of the window we're creating:
		const int windowWidth = windowRect.right - windowRect.left;
		const int windowHeight = windowRect.bottom - windowRect.top;
		// Note: We can't use the received width/height directly, as it may result in a window that's larger than the viewable area

		// Center the window within the screen. Clamp to 0, 0 for the top-left corner
		const int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
		const int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

		win32::Window::PlatformParams* platformParams = window.GetPlatformParams()->As<win32::Window::PlatformParams*>();

		std::wstring const& titleWideStr = util::ToWideString(createParams.m_title);

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
			&window // lpParam: A pointer that will be passed to the window through the CREATESTRUCT
		);
		SEAssert(platformParams->m_hWindow, "Failed to create hWnd");
		
		// Get window scaling:
		{
			::SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

			const HMONITOR monitor = MonitorFromWindow(platformParams->m_hWindow, MONITOR_DEFAULTTONEAREST);

			uint32_t dpiX = 0;
			uint32_t dpiY = 0;
			const HRESULT hr = ::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
			SEAssert(SUCCEEDED(hr), "Failed to get DPI for primary monitor");

			// The DPI of a 100% scaled monitor is 96; Thus DPI/96 = scale factor
			constexpr float k_dpi100PercentScale = 96.f;
			platformParams->m_windowScale = dpiY / k_dpi100PercentScale;

			const std::string scalingResults = std::format(
				"Display device reported DPI X/Y = ({}, {}). Assuming scaling factor = {}%%",
				dpiX,
				dpiY,
				platformParams->m_windowScale * 100.f);
			if (dpiX == dpiY)
			{
				LOG(scalingResults.c_str());
			}
			else
			{
				LOG_WARNING(scalingResults.c_str());
			}
		}

		::ShowWindow(platformParams->m_hWindow, SW_SHOW);
		::UpdateWindow(platformParams->m_hWindow);

		// Initialize the OLE (Object Linking and Embedding) library for the thread 
		const HRESULT hr = ::OleInitialize(NULL);
		platformParams->m_OLEIInitialized = SUCCEEDED(hr);
		SEAssert(platformParams->m_OLEIInitialized, "Failed to initialize OLE");

		// Register the window as a target for drag-and-drop operations:
		if (createParams.m_allowDragAndDrop && platformParams->m_OLEIInitialized)
		{
			platformParams->m_dropTarget = std::make_unique<SEWindowDropTarget>();

			::RegisterDragDrop(platformParams->m_hWindow, platformParams->m_dropTarget.get());
		}		

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


	void Window::Destroy(host::Window& window)
	{
		win32::Window::PlatformParams* platformParams = 
			window.GetPlatformParams()->As<win32::Window::PlatformParams*>();

		::DestroyWindow(platformParams->m_hWindow);

		platformParams->m_dropTarget = nullptr;

		// Uninitialize the OLE (Object Linking and Embedding) library for the thread
		if (platformParams->m_OLEIInitialized)
		{
			::OleUninitialize();
			platformParams->m_OLEIInitialized = false;
		}
	}


	void Window::SetRelativeMouseMode(host::Window const& window, bool relativeModeEnabled)
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


	SEWindowDropTarget::~SEWindowDropTarget()
	{
		Release();

		SEAssert(m_refCount.load() == 0, "SEWindowDropTarget destroyed with a non-zero ref count");
	}


	HRESULT SEWindowDropTarget::QueryInterface(REFIID riid, void** ppv)
	{
		if (riid == IID_IUnknown || riid == IID_IDropTarget)
		{
			*ppv = static_cast<IDropTarget*>(this);
			AddRef();
			return S_OK;
		}
		*ppv = nullptr;
		return E_NOINTERFACE;
	}


	ULONG SEWindowDropTarget::AddRef()
	{
		return ++m_refCount;
	}


	ULONG SEWindowDropTarget::Release()
	{
		return --m_refCount;
	}


	HRESULT SEWindowDropTarget::DragEnter(
		IDataObject* pDataObj,
		DWORD grfKeyState,
		POINTL pt,
		DWORD* pdwEffect)
	{
		*pdwEffect = DROPEFFECT_COPY;
		return S_OK;
	}


	HRESULT SEWindowDropTarget::DragOver(
		DWORD grfKeyState,
		POINTL pt,
		DWORD* pdwEffect)
	{
		*pdwEffect = DROPEFFECT_COPY;
		return S_OK;
	}


	HRESULT SEWindowDropTarget::DragLeave(void)
	{
		return S_OK;
	}


	HRESULT SEWindowDropTarget::Drop(
		IDataObject* pDataObj,	// Data object interface being transferred in the drag-and-drop operation
		DWORD grfKeyState,		// Current state of modifier keys MK_CONTROL/MK_SHIFT/MK_ALT/MK_BUTTON/MK_LBUTTON/MK_MBUTTON/MK_RBUTTON
		POINTL pt,				// Current cursor coordinates (in screen coordinates)
		DWORD* pdwEffect		// Input: pdwEffect parameter of the DoDragDrop function. Output: DROPEFFECT flag to indicate the result of the drop operation
	)
	{
		// Handle dropped files:
		FORMATETC format = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
		STGMEDIUM stg;
		if (SUCCEEDED(pDataObj->GetData(&format, &stg)))
		{
			const HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));

			if (hDrop)
			{
				const uint32_t fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

				for (uint32_t i = 0; i < fileCount; ++i)
				{
					// Get the current file path arg:
					wchar_t filePath[MAX_PATH];
					DragQueryFile(hDrop, i, filePath, MAX_PATH);

					// Convert it to a string, and send it as an event:
					std::string const& filePathStr = util::FromWideCString(filePath);

					core::EventManager::Get()->Notify(core::EventManager::EventInfo{
						.m_eventKey = eventkey::DragAndDrop,
						.m_data = filePathStr, });
				}

				GlobalUnlock(stg.hGlobal);
			}
			ReleaseStgMedium(&stg);
		}

		*pdwEffect = DROPEFFECT_COPY;
		return S_OK;
	}
}