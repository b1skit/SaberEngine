// Input key string defines. Used to share mappings between config keys/values and input managers

#pragma once

#include <SDL_keyboard.h>
#include <SDL_keycode.h>
#include <SDL_scancode.h>


namespace en
{
	// Adding new button input functionality to SaberEngine:
	// 1) Add the new button function to KeyboardInputButton and KeyboardInputButtonNames in this file
	// 2) Add a default button for this function in Config::InitializeDefaultValues()
	// 3) Add an event type to EventManager::EventType
	// 4) Fire an event in InputManager::Update()
	// 5) Subscribe to the event anywhere you want to react to the button press

	// Note: This macro is also used outside of this file (eg. Config.cpp)
	#define ENUM_TO_STR(x) #x

	// Buttons for specific functionality/controls (eg. forward, sprint, quit, etc)
	// These enums are also converted to strings by a pre-processor macro, and mapped to buttons in the config.cfg
	enum KeyboardInputButton
	{
		InputButton_Forward,
		InputButton_Backward,
		InputButton_Left,
		InputButton_Right,
		InputButton_Up,
		InputButton_Down,
		InputButton_Sprint,

		InputButton_Console,
		InputButton_Quit,

		KeyboardInputButton_Count
	};

	// KeyboardInputButton enum names, as strings.
	// Used to map functionality (eg. forward, sprint, quit, etc) to specific buttons in the config.cfg
	// Note: These MUST be in the same order as the KeyboardInputButton enum
	const std::string KeyboardInputButtonNames[KeyboardInputButton_Count] =
	{
		ENUM_TO_STR(InputButton_Forward),
		ENUM_TO_STR(InputButton_Backward),
		ENUM_TO_STR(InputButton_Left),
		ENUM_TO_STR(InputButton_Right),
		ENUM_TO_STR(InputButton_Up),
		ENUM_TO_STR(InputButton_Down),
		ENUM_TO_STR(InputButton_Sprint),

		ENUM_TO_STR(InputButton_Console),
		ENUM_TO_STR(InputButton_Quit),
	};

	enum MouseInputButton
	{
		InputMouse_Left,
		InputMouse_Right,

		MouseInputButton_Count
	};

	// Array of mouse button name strings: Used to iterate through all possible buttons
	// Note: These MUST be in the same order as the MouseInputButton enum
	const std::string MouseInputButtonNames[MouseInputButton_Count] =
	{
		ENUM_TO_STR(InputMouse_Left),
		ENUM_TO_STR(InputMouse_Right),
	};

	// Analogue controls (eg. mouse movement): Enums index mouseAxisStates array elements
	enum MouseInputAxis
	{
		Input_MouseX,
		Input_MouseY,

		MouseInputAxis_Count
	};
}