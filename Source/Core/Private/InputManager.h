// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Private/Definitions/KeyConfiguration.h"

#include "Private/Interfaces/IEngineComponent.h"
#include "Private/Interfaces/IEventListener.h"


namespace en
{
	class InputManager final : public virtual en::IEngineComponent, public virtual core::IEventListener
	{
	public:
		static InputManager* Get(); // Singleton functionality


	public:
		InputManager();

		InputManager(InputManager&&) noexcept = default;
		InputManager& operator=(InputManager&&) noexcept = default;
		~InputManager() = default;

		// Static member functions:
		static bool const& GetKeyboardInputState(definitions::KeyboardInputButton key);
		static bool const& GetMouseInputState(definitions::MouseInputButton button);
		static float GetRelativeMouseInput(definitions::MouseInputAxis axis);

		// IEngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(uint64_t frameNum, double stepTimeMs) override;

		// IEventListener interface:
		void HandleEvents() override;

	private:
		void LoadInputBindings();

		void InitializeKeyboardStates();
		void InitializeMouseAxisStates();
		void InitializeMouseButtonStates();


	private:
		static bool m_keyboardInputButtonStates[definitions::KeyboardInputButton_Count]; // Stores the state of keyboard keys
		static bool	m_mouseButtonStates[definitions::MouseInputButton_Count]; // Stores the state of mouse buttons
		static float m_mouseAxisStates[definitions::MouseInputAxis_Count]; // Mouse axis deltas

		std::unordered_map<definitions::SEKeycode, definitions::KeyboardInputButton> m_SEKeycodesToSEEventEnums;

		bool m_keyboardInputCaptured;
		bool m_mouseInputCaptured;


	private:
		InputManager(InputManager const&) = delete; // Disallow copying of our Singleton
		void operator=(InputManager const&) = delete;
	};
}


