// Input key string defines. Used to share mappings between config keys/values and input managers

#pragma once

#include <SDL_keyboard.h>
#include <SDL_keycode.h>
#include <SDL_scancode.h>

#include <unordered_map>
#include <string>

namespace en
{
	// Macros:
	#define MACRO_TO_STR(x) #x

	// Default true/false strings (Must be lowercase)
	#define TRUE_STRING		"true"		
	#define FALSE_STRING	"false"

	// Command strings: End with a space to maintain formatting
	#define SET_CMD		"set "		// Set a value
	#define BIND_CMD	"bind "		// Bind a key


	// Input binding names: Used as hash keys in EngineConfig <key, value> mappings
	#define InputButton_Forward		btn_forward
	#define InputButton_Backward	btn_backward
	#define InputButton_Left		btn_strafeLeft
	#define InputButton_Right		btn_strafeRight
	#define InputButton_Up			btn_up
	#define InputButton_Down		btn_down
	#define InputButton_Sprint		btn_sprint

	#define InputButton_Quit		btn_quit

	#define InputMouse_Left			mouse_left
	#define InputMouse_Right		mouse_right


	// Key binding names: Used as hash key names in InputManager to map keys to SDL ScanCodes
	#define L_SHIFT		"lshift"
	#define SPACE		"space"
	#define ESC			"esc"
	#define L_CTRL		"lctrl"


	const std::unordered_map<std::string, SDL_Scancode> ScancodeMappings =
	{
		{L_SHIFT,	SDL_SCANCODE_LSHIFT},
		{SPACE,		SDL_SCANCODE_SPACE},
		{ESC,		SDL_SCANCODE_ESCAPE},
		{L_CTRL,	SDL_SCANCODE_LCTRL},
	};


	// Binary controls: Enums index keyboardButtonStates array elements
	enum KeyboardButtonState
	{
		InputButton_Forward,
		InputButton_Backward,
		InputButton_Left,
		InputButton_Right,
		InputButton_Up,
		InputButton_Down,
		InputButton_Sprint,

		InputButton_Quit, // Temporary. TODO: Hard code a quit button

		KeyboardButtonState_Count
	};

	// Array of key name strings: Used to iterate through all possible buttons
	// Note: These MUST be in the same order as the KeyboardButtonState enum
	const std::string KEY_NAMES[KeyboardButtonState_Count] =
	{
		MACRO_TO_STR(InputButton_Forward),
		MACRO_TO_STR(InputButton_Backward),
		MACRO_TO_STR(InputButton_Left),
		MACRO_TO_STR(InputButton_Right),
		MACRO_TO_STR(InputButton_Up),
		MACRO_TO_STR(InputButton_Down),
		MACRO_TO_STR(InputButton_Sprint),

		MACRO_TO_STR(InputButton_Quit),
	};

	enum MouseButtonState
	{
		InputMouse_Left,
		InputMouse_Right,

		MouseButtonState_Count
	};

	// Array of mouse button name strings: Used to iterate through all possible buttons
	// Note: These MUST be in the same order as the MouseButtonState enum
	const std::string MouseButtonNames[MouseButtonState_Count] =
	{
		MACRO_TO_STR(InputMouse_Left),
		MACRO_TO_STR(InputMouse_Right),
	};

	// Analogue controls (eg. mouse movement): Enums index mouseAxisStates array elements
	enum InputAxis
	{
		Input_MouseX,
		Input_MouseY,

		InputAxis_Count
	};
}