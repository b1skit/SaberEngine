// © 2022 Adam Badke. All rights reserved.
#include "EngineApp.h"
#include "EventManager_Win32.h"
#include "EventManager.h"

#include "Core\Assert.h"


namespace win32
{
	void EventManager::ProcessMessages(en::EventManager& eventManager)
	{
		MSG msg;
		while (::PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
		{
			en::EventManager::EventInfo eventInfo;
			bool doBroadcastSEEvent = en::EngineApp::Get()->GetWindow()->GetFocusState();
			bool doTranslateAndDispatchWin32Msg = true;

			switch (msg.message)
			{
			case WM_DESTROY:
			case WM_CLOSE:
			case WM_QUIT:
			{
				eventInfo.m_type = en::EventManager::EventType::EngineQuit;
			}
			break;
			case WM_SYSCOMMAND:
			{
				// Maximize/minimize/restore/close buttons, or a command from the Window menu
				// https://learn.microsoft.com/en-us/windows/win32/menurc/wm-syscommand
				if (msg.wParam == SC_CLOSE)
				{
					eventInfo.m_type = en::EventManager::EventType::EngineQuit;
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
				doTranslateAndDispatchWin32Msg = false;
			}
			break;
			case WM_SYSKEYDOWN: // ALT + any key (aka a "system keypress"), or F10 (actives menu)
			case WM_KEYDOWN: // Non-system keypress (i.e. "normal" keypresses)
			case WM_SYSKEYUP:
			case WM_KEYUP:
			{
				eventInfo.m_type = en::EventManager::EventType::KeyEvent;

				switch (msg.wParam)
				{
				case VK_CONTROL:
				case VK_SHIFT:
				case VK_MENU: //alt
				{
					// Determine whether the left/right instance of control/shift/alt was pressed
					WORD vkCode = LOWORD(msg.wParam); // virtual-key code
					const WORD keyFlags = HIWORD(msg.lParam);
					WORD scanCode = LOBYTE(keyFlags);
					const BOOL isExtendedKey = (keyFlags & KF_EXTENDED) == KF_EXTENDED;
					if (isExtendedKey)
					{
						scanCode = MAKEWORD(scanCode, 0xE0);
					}
					vkCode = LOWORD(MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX));

					// Pack VK_LSHIFT/VK_RSHIFT/VK_LCONTROL/VK_RCONTROL/VK_LMENU/VK_RMENU
					eventInfo.m_data0.m_dataUI = static_cast<uint32_t>(vkCode);

					// We capture control/shift/alt to prevent them being interpreted as system keypresses
					doTranslateAndDispatchWin32Msg = false;
				}
				break;
				default: // Regular key press
				{
					eventInfo.m_data0.m_dataUI = static_cast<uint32_t>(msg.wParam); // Win32 virtual key code
				}
				}

				// Key is down if the most significant bit is set
				constexpr unsigned short k_mostSignificantBit = 1 << (std::numeric_limits<short>::digits);

				// true/false == pressed/released
				eventInfo.m_data1.m_dataB = 
					static_cast<bool>(GetAsyncKeyState(static_cast<int>(msg.wParam)) & k_mostSignificantBit);
			}
			break;
			case WM_CHAR:
			{
				// Posted when a WM_KEYDOWN message is translated by TranslateMessage
				eventInfo.m_type = en::EventManager::EventType::TextInputEvent;
				eventInfo.m_data0.m_dataC = static_cast<char>(msg.wParam);

				doTranslateAndDispatchWin32Msg = false;
			}
			break;
			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			{
				eventInfo.m_type = en::EventManager::EventType::MouseButtonEvent;

				switch (msg.message)
				{
				case WM_LBUTTONDOWN:
				case WM_LBUTTONUP:
				{
					eventInfo.m_data0.m_dataUI = 0;
					eventInfo.m_data1.m_dataB = (msg.message == WM_LBUTTONDOWN);
				}
				break;
				case WM_MBUTTONDOWN:
				case WM_MBUTTONUP:
				{
					eventInfo.m_data0.m_dataUI = 1;
					eventInfo.m_data1.m_dataB = (msg.message == WM_MBUTTONDOWN);
				}
				break;
				case WM_RBUTTONDOWN:
				case WM_RBUTTONUP:
				{
					eventInfo.m_data0.m_dataUI = 2;
					eventInfo.m_data1.m_dataB = (msg.message == WM_RBUTTONDOWN);
				}
				break;
				default:
				{
					SEAssertF("Invalid mouse button event");
				}
				}

				doTranslateAndDispatchWin32Msg = false;
			}
			break;
			case WM_MOUSEWHEEL:
			{
				eventInfo.m_type = en::EventManager::EventType::MouseWheelEvent;
				eventInfo.m_data0.m_dataI = 0; // X: Currently not supported
				eventInfo.m_data1.m_dataI = GET_WHEEL_DELTA_WPARAM(msg.wParam) / WHEEL_DELTA; // Y
				// Note: Wheel motion is in of +/- units WHEEL_DELTA == 120

				// Do not internally forward this message, as DefWindowProc propagates it up the parent chain until a
				// window that processes it is found
				// https://learn.microsoft.com/en-us/windows/win32/inputdev/about-mouse-input#mouse-messages
				doTranslateAndDispatchWin32Msg = false;
			}
			break;
			case WM_INPUT:
			{
				UINT dwSize = sizeof(RAWINPUT);
				static BYTE lpb[sizeof(RAWINPUT)];

				GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));

				RAWINPUT* raw = (RAWINPUT*)lpb;

				if (raw->header.dwType == RIM_TYPEMOUSE)
				{
					eventInfo.m_type = en::EventManager::EventType::MouseMotionEvent;

					eventInfo.m_data0.m_dataI = raw->data.mouse.lLastX;
					eventInfo.m_data1.m_dataI = raw->data.mouse.lLastY;
				}
				else
				{
					doBroadcastSEEvent = false;
				}
				doTranslateAndDispatchWin32Msg = false;
			}
			break;
			break;
			default:
			{
				doBroadcastSEEvent = false;
			}
			}

			if (doBroadcastSEEvent) // Post a Saber Engine Event
			{
				eventManager.Notify(std::move(eventInfo));
			}

			if (doTranslateAndDispatchWin32Msg) // Re-broadcast the message to our Window::WindowEventCallback handler
			{
				TranslateMessage(&msg);
				DispatchMessageA(&msg);
			}
		}
	}
}