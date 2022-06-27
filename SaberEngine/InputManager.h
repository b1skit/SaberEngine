// Input and control manager.
// Catches SDL events for SaberEngine


#pragma once
#include "EngineComponent.h"	// Base class
#include "EventListener.h"		// Base class
#include "KeyConfiguration.h"


namespace SaberEngine
{


	class InputManager : public EngineComponent, public EventListener
	{
	public:
		InputManager();

		// Singleton functionality:
		static InputManager& Instance();
		InputManager(InputManager const&)	= delete; // Disallow copying of our Singleton
		void operator=(InputManager const&) = delete;

		// Static member functions:
		static bool const&	GetKeyboardInputState(KEYBOARD_BUTTON_STATE key);
		static bool const&	GetMouseInputState(MOUSE_BUTTON_STATE button);
		static float		GetMouseAxisInput(INPUT_AXIS axis);

		// EngineComponent interface:
		void Startup();
		void Shutdown();
		void Update();

		// EventListener interface:
		void HandleEvent(EventInfo const* eventInfo);

		void LoadInputBindings();

	private:
		int				m_inputKeyboardBindings[INPUT_NUM_BUTTONS];		// Stores mapping from KEYBOARD_BUTTON_STATE enums to SDL_SCANCODE_ values
		static bool		m_keyboardButtonStates[INPUT_NUM_BUTTONS];		// Stores the state of keyboard keys

		static bool		m_mouseButtonStates[INPUT_MOUSE_NUM_BUTTONS];		// Stores the state of mouse buttons

		static float	m_mouseAxisStates[INPUT_NUM_INPUT_AXIS];			// Mouse axis deltas

		// Cache sensitivity params:
		static float m_mousePitchSensitivity;
		static float m_mouseYawSensitivity;
	};
}


