// Input and control manager.
// Catches SDL events for SaberEngine


#pragma once
#include "EngineComponent.h"	// Base class
#include "EventListener.h"		// Base class
#include "KeyConfiguration.h"


namespace SaberEngine
{


	class InputManager : public virtual EngineComponent, public virtual EventListener
	{
	public:
		InputManager();

		// Singleton functionality:
		InputManager(InputManager const&) = delete; // Disallow copying of our Singleton
		InputManager(InputManager&&) = delete;
		void operator=(InputManager const&) = delete;

		// Static member functions:
		static bool const&	GetKeyboardInputState(KEYBOARD_BUTTON_STATE key);
		static bool const&	GetMouseInputState(MOUSE_BUTTON_STATE button);
		static float		GetMouseAxisInput(INPUT_AXIS axis);

		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update() override;

		// EventListener interface:
		void HandleEvent(std::shared_ptr<EventInfo const> eventInfo) override;

		void LoadInputBindings();

	private:
		int				m_inputKeyboardBindings[INPUT_NUM_BUTTONS];		// Stores mapping from KEYBOARD_BUTTON_STATE enums to SDL_SCANCODE_ values
		static bool		m_keyboardButtonStates[INPUT_NUM_BUTTONS];		// Stores the state of keyboard keys

		static bool		m_mouseButtonStates[INPUT_MOUSE_NUM_BUTTONS];	// Stores the state of mouse buttons

		static float	m_mouseAxisStates[INPUT_NUM_INPUT_AXIS];		// Mouse axis deltas

		// Cache sensitivity params:
		static float m_mousePitchSensitivity;
		static float m_mouseYawSensitivity;
	};
}


