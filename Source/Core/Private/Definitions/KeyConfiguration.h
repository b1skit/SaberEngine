// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Private/../Util/CHashKey.h"


namespace definitions
{
	// Adding new button input functionality to SaberEngine:
	// 1) Add the new button function to KeyboardInputButton and KeyboardInputButtonNames in this file
	// 2) Add a default button for this function in Config::InitializeDefaultValues()
	// 3) Fire an event in InputManager::HandleEvents() when the associated button is pressed
	// 4) Subscribe to the event anywhere you want to react to the button press

	// Buttons for specific functionality/controls (eg. forward, sprint, quit, etc)
	// These enums are also converted to strings by a pre-processor macro, and mapped to buttons in the config file
	enum KeyboardInputButton : uint8_t
	{
		InputButton_Forward,
		InputButton_Backward,
		InputButton_Left,
		InputButton_Right,
		InputButton_Up,
		InputButton_Down,
		InputButton_Sprint,

		InputButton_ToggleUIVisibility,
		InputButton_Console,
		InputButton_VSync,

		KeyboardInputButton_Count
	};

	// KeyboardInputButton enum names, as strings.
	// Used to map functionality (eg. forward, sprint, quit, etc) to specific buttons in the config file
	// Note: These MUST be in the same order as the KeyboardInputButton enum
	constexpr util::CHashKey KeyboardInputButtonNames[KeyboardInputButton_Count] =
	{
		ENUM_TO_STR(InputButton_Forward),
		ENUM_TO_STR(InputButton_Backward),
		ENUM_TO_STR(InputButton_Left),
		ENUM_TO_STR(InputButton_Right),
		ENUM_TO_STR(InputButton_Up),
		ENUM_TO_STR(InputButton_Down),
		ENUM_TO_STR(InputButton_Sprint),

		ENUM_TO_STR(InputButton_ToggleUIVisibility),
		ENUM_TO_STR(InputButton_Console),
		ENUM_TO_STR(InputButton_VSync),
	};

	enum MouseInputButton : uint8_t
	{
		InputMouse_Left,
		InputMouse_Middle,
		InputMouse_Right,

		MouseInputButton_Count
	};

	// Analogue controls (eg. mouse movement): Enums index mouseAxisStates array elements
	enum MouseInputAxis : uint8_t
	{
		Input_MouseX,
		Input_MouseY,

		MouseInputAxis_Count
	};

	// Meaning-specific key codes (regardless of the location of a button/press)
	enum SEKeycode
	{
		SEK_F1,
		SEK_F2,
		SEK_F3,
		SEK_F4,
		SEK_F5,
		SEK_F6,
		SEK_F7,
		SEK_F8,
		SEK_F9,
		SEK_F10,
		SEK_F11,
		SEK_F12,

		SEK_1,
		SEK_2,
		SEK_3,
		SEK_4,
		SEK_5,
		SEK_6,
		SEK_7,
		SEK_8,
		SEK_9,
		SEK_0,

		SEK_A,
		SEK_B,
		SEK_C,
		SEK_D,
		SEK_E,
		SEK_F,
		SEK_G,
		SEK_H,
		SEK_I,
		SEK_J,
		SEK_K,
		SEK_L,
		SEK_M,
		SEK_N,
		SEK_O,
		SEK_P,
		SEK_Q,
		SEK_R,
		SEK_S,
		SEK_T,
		SEK_U,
		SEK_V,
		SEK_W,
		SEK_X,
		SEK_Y,
		SEK_Z,

		SEK_RETURN,
		SEK_ESCAPE,
		SEK_BACKSPACE,
		SEK_TAB,
		SEK_SPACE,

		SEK_MINUS,
		SEK_EQUALS,
		SEK_LEFTBRACKET,
		SEK_RIGHTBRACKET,

		SEK_BACKSLASH, // "\"

		SEK_SEMICOLON,
		SEK_APOSTROPHE,
		SEK_GRAVE, // "`", aka Tilde
		SEK_COMMA,
		SEK_PERIOD,
		SEK_SLASH, // "/"

		SEK_CAPSLOCK,

		SEK_PRINTSCREEN,
		SEK_SCROLLLOCK,
		SEK_PAUSE,
		SEK_INSERT,

		SEK_HOME,
		SEK_PAGEUP,
		SEK_DELETE,
		SEK_END,
		SEK_PAGEDOWN,

		SEK_RIGHT,
		SEK_LEFT,
		SEK_DOWN,
		SEK_UP,

		SEK_NUMLOCK,

		SEK_APPLICATION, // Windows key

		SEK_LCTRL,
		SEK_LSHIFT,
		SEK_LALT,
		SEK_RCTRL,
		SEK_RSHIFT,
		SEK_RALT,

		SEK_UNKNOWN,
		SaberEngineKeycodes_Count = SEK_UNKNOWN
	};
	static_assert(SaberEngineKeycodes_Count < 256);

	

	// Map strings used in config file to their SEKeycode enum value
	inline SEKeycode GetSEKeycodeFromName(std::string const& keyname)
	{
		static const std::unordered_map<std::string, SEKeycode> SEKeyNamesToKeycodes =
		{
			{"F1", SEK_F1},
			{"F2", SEK_F2},
			{"F3", SEK_F3},
			{"F4", SEK_F4},
			{"F5", SEK_F5},
			{"F6", SEK_F6},
			{"F7", SEK_F7},
			{"F8", SEK_F8},
			{"F9", SEK_F9},
			{"F10", SEK_F10},
			{"F11", SEK_F11},
			{"F12", SEK_F12},
			{"1", SEK_1},
			{"2", SEK_2},
			{"3", SEK_3},
			{"4", SEK_4},
			{"5", SEK_5},
			{"6", SEK_6},
			{"7", SEK_7},
			{"8", SEK_8},
			{"9", SEK_9},
			{"0", SEK_0},
			{"a", SEK_A},
			{"b", SEK_B},
			{"c", SEK_C},
			{"d", SEK_D},
			{"e", SEK_E},
			{"f", SEK_F},
			{"g", SEK_G},
			{"h", SEK_H},
			{"i", SEK_I},
			{"j", SEK_J},
			{"k", SEK_K},
			{"l", SEK_L},
			{"m", SEK_M},
			{"n", SEK_N},
			{"o", SEK_O},
			{"p", SEK_P},
			{"q", SEK_Q},
			{"r", SEK_R},
			{"s", SEK_S},
			{"t", SEK_T},
			{"u", SEK_U},
			{"v", SEK_V},
			{"w", SEK_W},
			{"x", SEK_X},
			{"y", SEK_Y},
			{"z", SEK_Z},
			{"Return", SEK_RETURN},
			{"Escape", SEK_ESCAPE},
			{"Backspace", SEK_BACKSPACE},
			{"Tab", SEK_TAB},
			{"Space", SEK_SPACE},
			{"Minus", SEK_MINUS},
			{"Equals", SEK_EQUALS},
			{"Left Bracket", SEK_LEFTBRACKET},
			{"Right Bracket", SEK_RIGHTBRACKET},
			{"Backslash", SEK_BACKSLASH},
			{"Semicolon", SEK_SEMICOLON},
			{"Apostrophe", SEK_APOSTROPHE},
			{"Grave", SEK_GRAVE},
			{"Comma", SEK_COMMA},
			{"Period", SEK_PERIOD},
			{"Slash", SEK_SLASH},
			{"Caps Lock", SEK_CAPSLOCK},
			{"Print Screen", SEK_PRINTSCREEN},
			{"Scroll Lock", SEK_SCROLLLOCK},
			{"Pause", SEK_PAUSE},
			{"Insert", SEK_INSERT},
			{"Home", SEK_HOME},
			{"Page Up", SEK_PAGEUP},
			{"Delete", SEK_DELETE},
			{"End", SEK_END},
			{"Page Down", SEK_PAGEDOWN},
			{"Right", SEK_RIGHT},
			{"Left", SEK_LEFT},
			{"Down", SEK_DOWN},
			{"Up", SEK_UP},
			{"Num Lock", SEK_NUMLOCK},
			{"Application", SEK_APPLICATION},
			{"Left Ctrl", SEK_LCTRL},
			{"Left Shift", SEK_LSHIFT},
			{"Left Alt", SEK_LALT},
			{"Right Ctrl", SEK_RCTRL},
			{"Right Shift", SEK_RSHIFT},
			{"Right Alt", SEK_RALT},
		};
	
		auto const& result = SEKeyNamesToKeycodes.find(keyname.c_str());
		if (result != SEKeyNamesToKeycodes.end())
		{
			return result->second;
		}
		else
		{
			return SEKeycode::SEK_UNKNOWN;
		}
	}
}