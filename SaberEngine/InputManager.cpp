#include <SDL.h>

#include "InputManager.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"
#include "EventManager.h"

using en::CoreEngine;
using en::EventManager;


namespace en
{
	// Static members:
	bool InputManager::m_keyboardButtonStates[en::KeyboardButtonState_Count];
	bool InputManager::m_mouseButtonStates[en::MouseButtonState_Count];
	float InputManager::m_mouseAxisStates[en::InputAxis_Count];

	float InputManager::m_mousePitchSensitivity	= -0.00005f;
	float InputManager::m_mouseYawSensitivity	= -0.00005f;


	// Constructor:
	InputManager::InputManager() : EngineComponent("InputManager")
	{
		// Initialize keyboard keys:
		for (int i = 0; i < en::KeyboardButtonState_Count; i++)
		{
			m_inputKeyboardBindings[i]		= SDL_SCANCODE_UNKNOWN; // == 0
			m_keyboardButtonStates[i]		= false;
		}

		// Initialize mouse axes:
		for (int i = 0; i < en::InputAxis_Count; i++)
		{
			m_mouseAxisStates[i]	= 0.0f;
		}
	}


	bool const& InputManager::GetKeyboardInputState(en::KeyboardButtonState key)
	{
		return InputManager::m_keyboardButtonStates[key];
	}


	bool const& InputManager::GetMouseInputState(en::MouseButtonState button)
	{
		return InputManager::m_mouseButtonStates[button];
	}


	float InputManager::GetMouseAxisInput(en::InputAxis axis)
	{
		float sensitivity;
		if (axis == en::Input_MouseX)
		{
			sensitivity = m_mousePitchSensitivity;
		}
		else
		{
			sensitivity = m_mouseYawSensitivity;
		}

		return InputManager::m_mouseAxisStates[axis] * sensitivity;
	}
	

	void InputManager::Startup()
	{
		LOG("InputManager starting...");

		LoadInputBindings();

		// Cache sensitivity params. For whatever reason, we must multiply by -1 (we store positive values for sanity)
		InputManager::m_mousePitchSensitivity =
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("mousePitchSensitivity") * -1.0f;
		InputManager::m_mouseYawSensitivity	= 
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("mouseYawSensitivity") * -1.0f;
	}


	void InputManager::Shutdown()
	{
		LOG("Input manager shutting down...");
	}


	void InputManager::Update()
	{
		// Update keyboard states:
		const Uint8* SDLKeyboardState = SDL_GetKeyboardState(NULL);

		m_keyboardButtonStates[en::InputButton_Forward] =
			(bool)SDLKeyboardState[m_inputKeyboardBindings[en::InputButton_Forward]];
		m_keyboardButtonStates[en::InputButton_Backward] =
			(bool)SDLKeyboardState[m_inputKeyboardBindings[en::InputButton_Backward]];
		m_keyboardButtonStates[en::InputButton_Left] =
			(bool)SDLKeyboardState[m_inputKeyboardBindings[en::InputButton_Left]];
		m_keyboardButtonStates[en::InputButton_Right] =
			(bool)SDLKeyboardState[m_inputKeyboardBindings[en::InputButton_Right]];
		m_keyboardButtonStates[en::InputButton_Up]	=
			(bool)SDLKeyboardState[m_inputKeyboardBindings[en::InputButton_Up]];
		m_keyboardButtonStates[en::InputButton_Down] =
			(bool)SDLKeyboardState[m_inputKeyboardBindings[en::InputButton_Down]];

		m_keyboardButtonStates[en::InputButton_Quit] =
			(bool)SDLKeyboardState[m_inputKeyboardBindings[en::InputButton_Quit]];


		// Update mouse button states:
		m_mouseButtonStates[en::InputMouse_Left] =
			(bool)(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT));
		m_mouseButtonStates[en::InputMouse_Right] =
			(bool)(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT));


		// Get the mouse deltas, once per frame:
		int xRel, yRel = 0;
		SDL_GetRelativeMouseState(&xRel, &yRel);
		m_mouseAxisStates[en::Input_MouseX] = (float)xRel;
		m_mouseAxisStates[en::Input_MouseY] = (float)yRel;
	}


	void InputManager::HandleEvent(std::shared_ptr<EventManager::EventInfo const> eventInfo)
	{
		
	}


	void InputManager::LoadInputBindings()
	{
		for (int i = 0; i < en::KeyboardButtonState_Count; i++)
		{
			SDL_Scancode theScancode;

			string buttonString = CoreEngine::GetCoreEngine()->GetConfig()->GetValueAsString(en::KEY_NAMES[i]);

			// Handle chars:
			if (buttonString.length() == 1)
			{
				theScancode = SDL_GetScancodeFromKey((SDL_Keycode)buttonString.c_str()[0]);
			}
			// Handle strings:
			else
			{
				auto result = en::ScancodeMappings.find(buttonString);
				if (result != en::ScancodeMappings.end())
				{
					theScancode = result->second;
				}
			}

			m_inputKeyboardBindings[i] = theScancode;
		}
	}
}

