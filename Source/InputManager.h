// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "EngineComponent.h"
#include "EventManager.h"
#include "EventListener.h"
#include "KeyConfiguration.h"


namespace en
{
	class InputManager final : public virtual en::EngineComponent, public virtual en::EventListener
	{
	public:
		static InputManager* Get(); // Singleton functionality


	public:
		InputManager();
		~InputManager() = default;

		// Static member functions:
		static bool const& GetKeyboardInputState(en::KeyboardInputButton key);
		static bool const& GetMouseInputState(en::MouseInputButton button);
		static float GetRelativeMouseInput(en::MouseInputAxis axis);

		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(uint64_t frameNum, double stepTimeMs) override;

		// EventListener interface:
		void HandleEvents() override;

	private:
		void LoadInputBindings();

	private:
		static bool m_keyboardInputButtonStates[en::KeyboardInputButton_Count]; // Stores the state of keyboard keys
		static bool	m_mouseButtonStates[en::MouseInputButton_Count]; // Stores the state of mouse buttons
		static float m_mouseAxisStates[en::MouseInputAxis_Count]; // Mouse axis deltas

		std::unordered_map<SEKeycode, en::KeyboardInputButton> m_SEKeycodesToSEEventEnums;


		// Sensitivity params:
		static float m_mousePitchSensitivity;
		static float m_mouseYawSensitivity;

		re::Context const* m_context;

		bool m_consoleTriggered; // Is the console menu currently holding focus?
		bool m_prevConsoleTriggeredState;

	private:
		InputManager(InputManager const&) = delete; // Disallow copying of our Singleton
		InputManager(InputManager&&) = delete;
		void operator=(InputManager const&) = delete;
	};
}


