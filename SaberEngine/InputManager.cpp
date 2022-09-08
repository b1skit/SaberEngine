#include "InputManager.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"

using en::CoreEngine;


namespace SaberEngine
{
	// Static members:
	bool InputManager::m_keyboardButtonStates[INPUT_NUM_BUTTONS];
	bool InputManager::m_mouseButtonStates[INPUT_MOUSE_NUM_BUTTONS];
	float InputManager::m_mouseAxisStates[INPUT_NUM_INPUT_AXIS];

	float InputManager::m_mousePitchSensitivity	= -0.00005f;
	float InputManager::m_mouseYawSensitivity	= -0.00005f;


	// Constructor:
	InputManager::InputManager() : EngineComponent("InputManager")
	{
		// Initialize keyboard keys:
		for (int i = 0; i < INPUT_NUM_BUTTONS; i++)
		{
			m_inputKeyboardBindings[i]		= SDL_SCANCODE_UNKNOWN; // == 0
			m_keyboardButtonStates[i]		= false;
		}

		// Initialize mouse axes:
		for (int i = 0; i < INPUT_NUM_INPUT_AXIS; i++)
		{
			m_mouseAxisStates[i]	= 0.0f;
		}
	}


	bool const& InputManager::GetKeyboardInputState(KEYBOARD_BUTTON_STATE key)
	{
		return InputManager::m_keyboardButtonStates[key];
	}


	bool const& InputManager::GetMouseInputState(MOUSE_BUTTON_STATE button)
	{
		return InputManager::m_mouseButtonStates[button];
	}


	float InputManager::GetMouseAxisInput(INPUT_AXIS axis)
	{
		float sensitivity;
		if (axis == INPUT_MOUSE_X)
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

		m_keyboardButtonStates[INPUT_BUTTON_FORWARD] = 
			(bool)SDLKeyboardState[m_inputKeyboardBindings[INPUT_BUTTON_FORWARD]];
		m_keyboardButtonStates[INPUT_BUTTON_BACKWARD] = 
			(bool)SDLKeyboardState[m_inputKeyboardBindings[INPUT_BUTTON_BACKWARD]];
		m_keyboardButtonStates[INPUT_BUTTON_LEFT] = 
			(bool)SDLKeyboardState[m_inputKeyboardBindings[INPUT_BUTTON_LEFT]];
		m_keyboardButtonStates[INPUT_BUTTON_RIGHT] = 
			(bool)SDLKeyboardState[m_inputKeyboardBindings[INPUT_BUTTON_RIGHT]];
		m_keyboardButtonStates[INPUT_BUTTON_UP]	= 
			(bool)SDLKeyboardState[m_inputKeyboardBindings[INPUT_BUTTON_UP]];
		m_keyboardButtonStates[INPUT_BUTTON_DOWN] = 
			(bool)SDLKeyboardState[m_inputKeyboardBindings[INPUT_BUTTON_DOWN]];

		m_keyboardButtonStates[INPUT_BUTTON_QUIT] = 
			(bool)SDLKeyboardState[m_inputKeyboardBindings[INPUT_BUTTON_QUIT]];


		// Update mouse button states:
		m_mouseButtonStates[INPUT_MOUSE_LEFT] = 
			(bool)(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT));
		m_mouseButtonStates[INPUT_MOUSE_RIGHT] =
			(bool)(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT));


		// Get the mouse deltas, once per frame:
		int xRel, yRel = 0;
		SDL_GetRelativeMouseState(&xRel, &yRel);
		m_mouseAxisStates[INPUT_MOUSE_X] = (float)xRel;
		m_mouseAxisStates[INPUT_MOUSE_Y] = (float)yRel;
	}


	void InputManager::HandleEvent(std::shared_ptr<EventInfo const> eventInfo)
	{
		
	}


	void InputManager::LoadInputBindings()
	{
		for (int i = 0; i < INPUT_NUM_BUTTONS; i++)
		{
			SDL_Scancode theScancode;

			string buttonString = CoreEngine::GetCoreEngine()->GetConfig()->GetValueAsString(KEY_NAMES[i]);

			// Handle chars:
			if (buttonString.length() == 1)
			{
				theScancode = SDL_GetScancodeFromKey((SDL_Keycode)buttonString.c_str()[0]);
			}
			// Handle strings:
			else
			{
				auto result = SCANCODE_MAPPINGS.find(buttonString);
				if (result != SCANCODE_MAPPINGS.end())
				{
					theScancode = result->second;
				}
			}

			m_inputKeyboardBindings[i] = theScancode;
		}
	}
}

