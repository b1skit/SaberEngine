// © 2022 Adam Badke. All rights reserved.
#include "InputManager_Win32.h"
#include "InputManager.h"

#include "Definitions/KeyConfiguration.h"


namespace win32
{
	void InputManager::Startup(en::InputManager& inputManager)
	{
		//
	}


	definitions::SEKeycode InputManager::ConvertToSEKeycode(uint32_t platKeycode)
	{
		switch (platKeycode)
		{
		case VK_F1: return definitions::SEK_F1;
		case VK_F2: return definitions::SEK_F2;
		case VK_F3: return definitions::SEK_F3;
		case VK_F4: return definitions::SEK_F4;
		case VK_F5: return definitions::SEK_F5;
		case VK_F6: return definitions::SEK_F6;
		case VK_F7: return definitions::SEK_F7;
		case VK_F8: return definitions::SEK_F8;
		case VK_F9: return definitions::SEK_F9;
		case VK_F10: return definitions::SEK_F10;
		case VK_F11: return definitions::SEK_F11;
		case VK_F12: return definitions::SEK_F12;

		case 0x30: return definitions::SEK_0;
		case 0x31: return definitions::SEK_1;
		case 0x32: return definitions::SEK_2;
		case 0x33: return definitions::SEK_3;
		case 0x34: return definitions::SEK_4;
		case 0x35: return definitions::SEK_5;
		case 0x36: return definitions::SEK_6;
		case 0x37: return definitions::SEK_7;
		case 0x38: return definitions::SEK_8;
		case 0x39: return definitions::SEK_9;

		case 0x41: return definitions::SEK_A;
		case 0x42: return definitions::SEK_B;
		case 0x43: return definitions::SEK_C;
		case 0x44: return definitions::SEK_D;
		case 0x45: return definitions::SEK_E;
		case 0x46: return definitions::SEK_F;
		case 0x47: return definitions::SEK_G;
		case 0x48: return definitions::SEK_H;
		case 0x49: return definitions::SEK_I;
		case 0x4A: return definitions::SEK_J;
		case 0x4B: return definitions::SEK_K;
		case 0x4C: return definitions::SEK_L;
		case 0x4D: return definitions::SEK_M;
		case 0x4E: return definitions::SEK_N;
		case 0x4F: return definitions::SEK_O;
		case 0x50: return definitions::SEK_P;
		case 0x51: return definitions::SEK_Q;
		case 0x52: return definitions::SEK_R;
		case 0x53: return definitions::SEK_S;
		case 0x54: return definitions::SEK_T;
		case 0x55: return definitions::SEK_U;
		case 0x56: return definitions::SEK_V;
		case 0x57: return definitions::SEK_W;
		case 0x58: return definitions::SEK_X;
		case 0x59: return definitions::SEK_Y;
		case 0x5A: return definitions::SEK_Z;

		case VK_RETURN: return definitions::SEK_RETURN;
		case VK_ESCAPE: return definitions::SEK_ESCAPE;
		case VK_BACK: return definitions::SEK_BACKSPACE;
		case VK_TAB: return definitions::SEK_TAB;
		case VK_SPACE: return definitions::SEK_SPACE;

		case VK_OEM_MINUS: return definitions::SEK_MINUS;
		case VK_OEM_PLUS: return definitions::SEK_EQUALS;
		case VK_OEM_4: return definitions::SEK_LEFTBRACKET;
		case VK_OEM_6: return definitions::SEK_RIGHTBRACKET;

		case VK_OEM_5: return definitions::SEK_BACKSLASH;

		case VK_OEM_1: return definitions::SEK_SEMICOLON;
		case VK_OEM_7: return definitions::SEK_APOSTROPHE;
		case VK_OEM_3: return definitions::SEK_GRAVE;
		case VK_OEM_COMMA: return definitions::SEK_COMMA;
		case VK_OEM_PERIOD: return definitions::SEK_PERIOD;
		case VK_OEM_2: return definitions::SEK_SLASH;

		case VK_CAPITAL: return definitions::SEK_CAPSLOCK;

		case VK_SNAPSHOT: return definitions::SEK_PRINTSCREEN;
		case VK_SCROLL: return definitions::SEK_SCROLLLOCK;
		case VK_PAUSE: return definitions::SEK_PAUSE;
		case VK_INSERT: return definitions::SEK_INSERT;

		case VK_HOME: return definitions::SEK_HOME;
		case VK_PRIOR: return definitions::SEK_PAGEUP;
		case VK_DELETE: return definitions::SEK_DELETE;
		case VK_END: return definitions::SEK_END;
		case VK_NEXT: return definitions::SEK_PAGEDOWN;

		case VK_RIGHT: return definitions::SEK_RIGHT;
		case VK_LEFT: return definitions::SEK_LEFT;
		case VK_DOWN: return definitions::SEK_DOWN;
		case VK_UP: return definitions::SEK_UP;

		case VK_NUMLOCK: return definitions::SEK_NUMLOCK;

		case VK_APPS: return definitions::SEK_APPLICATION;

		case VK_LCONTROL: return definitions::SEK_LCTRL;
		case VK_LSHIFT: return definitions::SEK_LSHIFT;
		case VK_LMENU: return definitions::SEK_LALT;
		case VK_RCONTROL: return definitions::SEK_RCTRL;
		case VK_RSHIFT: return definitions::SEK_RSHIFT;
		case VK_RMENU: return definitions::SEK_RALT;

		default: return definitions::SEK_UNKNOWN;
		}
	}
}