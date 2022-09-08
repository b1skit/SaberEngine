#pragma once

#include "EngineComponent.h"
#include "EventListener.h"
#include "KeyConfiguration.h"


namespace fr
{
	class InputManager : public virtual SaberEngine::EngineComponent, public virtual SaberEngine::EventListener
	{
	public:
		InputManager();
		~InputManager() = default;

		// Singleton functionality:
		InputManager(InputManager const&) = delete; // Disallow copying of our Singleton
		InputManager(InputManager&&) = delete;
		void operator=(InputManager const&) = delete;

		// Static member functions:
		static bool const& GetKeyboardInputState(SaberEngine::KEYBOARD_BUTTON_STATE key);
		static bool const& GetMouseInputState(SaberEngine::MOUSE_BUTTON_STATE button);
		static float GetMouseAxisInput(SaberEngine::INPUT_AXIS axis);

		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update() override;

		// EventListener interface:
		void HandleEvent(std::shared_ptr<SaberEngine::EventInfo const> eventInfo) override;

	private:
		void LoadInputBindings();

		int	 m_inputKeyboardBindings[SaberEngine::INPUT_NUM_BUTTONS]; // Maps from KEYBOARD_BUTTON_STATE enums to SDL_SCANCODE_ values
		static bool m_keyboardButtonStates[SaberEngine::INPUT_NUM_BUTTONS]; // Stores the state of keyboard keys

		static bool	m_mouseButtonStates[SaberEngine::INPUT_MOUSE_NUM_BUTTONS]; // Stores the state of mouse buttons

		static float m_mouseAxisStates[SaberEngine::INPUT_NUM_INPUT_AXIS]; // Mouse axis deltas

		// Cache sensitivity params:
		static float m_mousePitchSensitivity;
		static float m_mouseYawSensitivity;
	};
}


