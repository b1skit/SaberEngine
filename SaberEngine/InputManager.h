#pragma once

#include "EngineComponent.h"
#include "EventListener.h"
#include "KeyConfiguration.h"
#include "Context.h"


namespace en
{
	

	class InputManager : public virtual en::EngineComponent, public virtual en::EventListener
	{
	public:
		InputManager();
		~InputManager() = default;

		// Singleton functionality:
		InputManager(InputManager const&) = delete; // Disallow copying of our Singleton
		InputManager(InputManager&&) = delete;
		void operator=(InputManager const&) = delete;

		// Static member functions:
		static bool const& GetKeyboardInputState(en::KeyboardButtonState key);
		static bool const& GetMouseInputState(en::MouseButtonState button);
		static float GetMouseAxisInput(en::InputAxis axis);

		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update() override;

		// EventListener interface:
		void HandleEvent(std::shared_ptr<en::EventManager::EventInfo const> eventInfo) override;

	private:
		void LoadInputBindings();

		int	 m_inputKeyboardBindings[en::KeyboardButtonState_Count]; // Maps from KeyboardButtonState enums to SDL_SCANCODE_ values
		static bool m_keyboardButtonStates[en::KeyboardButtonState_Count]; // Stores the state of keyboard keys

		static bool	m_mouseButtonStates[en::MouseButtonState_Count]; // Stores the state of mouse buttons

		static float m_mouseAxisStates[en::InputAxis_Count]; // Mouse axis deltas

		// Cache sensitivity params:
		static float m_mousePitchSensitivity;
		static float m_mouseYawSensitivity;

		re::Context const* m_context;
	};
}


