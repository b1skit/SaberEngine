// © 2022 Adam Badke. All rights reserved.
#include "InputManager_Win32.h"
#include "InputManager.h"
#include "KeyConfiguration.h"
#include "RenderManager.h"
#include "Window_Win32.h"


namespace win32
{
	void InputManager::Startup(en::InputManager& inputManager)
	{
		// Register the mouse as a raw input device:
		// https://learn.microsoft.com/en-us/windows/win32/dxtecharts/taking-advantage-of-high-dpi-mouse-movement?redirectedfrom=MSDN

		RAWINPUTDEVICE rawInputDevice[1];
		rawInputDevice[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
		rawInputDevice[0].usUsage = HID_USAGE_GENERIC_MOUSE;
		rawInputDevice[0].dwFlags = RIDEV_INPUTSINK;

		win32::Window::PlatformParams* const windowPlatformParams =
			dynamic_cast<win32::Window::PlatformParams*>(re::RenderManager::Get()->GetContext().GetWindow()->GetPlatformParams());

		rawInputDevice[0].hwndTarget = windowPlatformParams->m_hWindow;
		RegisterRawInputDevices(rawInputDevice, 1, sizeof(rawInputDevice[0]));
	}


	en::SEKeycode InputManager::ConvertToSEKeycode(uint32_t platKeycode)
	{
		switch (platKeycode)
		{
		case VK_F1: return en::SEK_F1;
		case VK_F2: return en::SEK_F2;
		case VK_F3: return en::SEK_F3;
		case VK_F4: return en::SEK_F4;
		case VK_F5: return en::SEK_F5;
		case VK_F6: return en::SEK_F6;
		case VK_F7: return en::SEK_F7;
		case VK_F8: return en::SEK_F8;
		case VK_F9: return en::SEK_F9;
		case VK_F10: return en::SEK_F10;
		case VK_F11: return en::SEK_F11;
		case VK_F12: return en::SEK_F12;

		case 0x30: return en::SEK_0;
		case 0x31: return en::SEK_1;
		case 0x32: return en::SEK_2;
		case 0x33: return en::SEK_3;
		case 0x34: return en::SEK_4;
		case 0x35: return en::SEK_5;
		case 0x36: return en::SEK_6;
		case 0x37: return en::SEK_7;
		case 0x38: return en::SEK_8;
		case 0x39: return en::SEK_9;

		case 0x41: return en::SEK_A;
		case 0x42: return en::SEK_B;
		case 0x43: return en::SEK_C;
		case 0x44: return en::SEK_D;
		case 0x45: return en::SEK_E;
		case 0x46: return en::SEK_F;
		case 0x47: return en::SEK_G;
		case 0x48: return en::SEK_H;
		case 0x49: return en::SEK_I;
		case 0x4A: return en::SEK_J;
		case 0x4B: return en::SEK_K;
		case 0x4C: return en::SEK_L;
		case 0x4D: return en::SEK_M;
		case 0x4E: return en::SEK_N;
		case 0x4F: return en::SEK_O;
		case 0x50: return en::SEK_P;
		case 0x51: return en::SEK_Q;
		case 0x52: return en::SEK_R;
		case 0x53: return en::SEK_S;
		case 0x54: return en::SEK_T;
		case 0x55: return en::SEK_U;
		case 0x56: return en::SEK_V;
		case 0x57: return en::SEK_W;
		case 0x58: return en::SEK_X;
		case 0x59: return en::SEK_Y;
		case 0x5A: return en::SEK_Z;

		case VK_RETURN: return en::SEK_RETURN;
		case VK_ESCAPE: return en::SEK_ESCAPE;
		case VK_BACK: return en::SEK_BACKSPACE;
		case VK_TAB: return en::SEK_TAB;
		case VK_SPACE: return en::SEK_SPACE;

		case VK_OEM_MINUS: return en::SEK_MINUS;
		case VK_OEM_PLUS: return en::SEK_EQUALS;
		case VK_OEM_4: return en::SEK_LEFTBRACKET;
		case VK_OEM_6: return en::SEK_RIGHTBRACKET;

		case VK_OEM_5: return en::SEK_BACKSLASH;

		case VK_OEM_1: return en::SEK_SEMICOLON;
		case VK_OEM_7: return en::SEK_APOSTROPHE;
		case VK_OEM_3: return en::SEK_GRAVE;
		case VK_OEM_COMMA: return en::SEK_COMMA;
		case VK_OEM_PERIOD: return en::SEK_PERIOD;
		case VK_OEM_2: return en::SEK_SLASH;

		case VK_CAPITAL: return en::SEK_CAPSLOCK;

		case VK_SNAPSHOT: return en::SEK_PRINTSCREEN;
		case VK_SCROLL: return en::SEK_SCROLLLOCK;
		case VK_PAUSE: return en::SEK_PAUSE;
		case VK_INSERT: return en::SEK_INSERT;

		case VK_HOME: return en::SEK_HOME;
		case VK_PRIOR: return en::SEK_PAGEUP;
		case VK_DELETE: return en::SEK_DELETE;
		case VK_END: return en::SEK_END;
		case VK_NEXT: return en::SEK_PAGEDOWN;

		case VK_RIGHT: return en::SEK_RIGHT;
		case VK_LEFT: return en::SEK_LEFT;
		case VK_DOWN: return en::SEK_DOWN;
		case VK_UP: return en::SEK_UP;

		case VK_NUMLOCK: return en::SEK_NUMLOCK;

		case VK_APPS: return en::SEK_APPLICATION;

		case VK_LCONTROL: return en::SEK_LCTRL;
		case VK_LSHIFT: return en::SEK_LSHIFT;
		case VK_LMENU: return en::SEK_LALT;
		case VK_RCONTROL: return en::SEK_RCTRL;
		case VK_RSHIFT: return en::SEK_RSHIFT;
		case VK_RMENU: return en::SEK_RALT;

		default: return en::SEK_UNKNOWN;
		}
	}
}