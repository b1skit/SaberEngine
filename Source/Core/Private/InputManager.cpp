// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Config.h"
#include "EventManager.h"
#include "InputManager.h"
#include "InputManager_Platform.h"

#include "Definitions/EventKeys.h"
#include "Definitions/KeyConfiguration.h"


namespace en
{
	// Static members:
	bool InputManager::m_keyboardInputButtonStates[definitions::KeyboardInputButton_Count];
	bool InputManager::m_mouseButtonStates[definitions::MouseInputButton_Count];
	float InputManager::m_mouseAxisStates[definitions::MouseInputAxis_Count];


	InputManager* InputManager::Get()
	{
		static std::unique_ptr<en::InputManager> instance = std::make_unique<en::InputManager>();
		return instance.get();
	}


	InputManager::InputManager()
		: m_keyboardInputCaptured(false)
		, m_mouseInputCaptured(false)
	{
		InitializeKeyboardStates();
		InitializeMouseAxisStates();
	}


	bool const& InputManager::GetKeyboardInputState(definitions::KeyboardInputButton key)
	{
		return m_keyboardInputButtonStates[key];
	}


	bool const& InputManager::GetMouseInputState(definitions::MouseInputButton button)
	{
		return m_mouseButtonStates[button];
	}


	float InputManager::GetRelativeMouseInput(definitions::MouseInputAxis axis)
	{
		return m_mouseAxisStates[axis];
	}
	

	void InputManager::Startup()
	{
		LOG("InputManager starting...");

		LoadInputBindings();

		// Event subscriptions:
		core::EventManager::Get()->Subscribe(eventkey::KeyEvent, this);
		core::EventManager::Get()->Subscribe(eventkey::MouseMotionEvent, this);
		core::EventManager::Get()->Subscribe(eventkey::MouseButtonEvent, this);
		core::EventManager::Get()->Subscribe(eventkey::MouseWheelEvent, this);
		core::EventManager::Get()->Subscribe(eventkey::WindowFocusChanged, this);
		core::EventManager::Get()->Subscribe(eventkey::KeyboardInputCaptureChange, this);
		core::EventManager::Get()->Subscribe(eventkey::MouseInputCaptureChange, this);

		platform::InputManager::Startup(*this);
	}


	void InputManager::Shutdown()
	{
		LOG("Input manager shutting down...");
	}


	void InputManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		// Prepare for the next around of input events fired by the EventManager
		InitializeMouseAxisStates();
		InitializeMouseButtonStates();

		HandleEvents();
	}


	void InputManager::HandleEvents()
	{
		while (HasEvents())
		{
			core::EventManager::EventInfo const& eventInfo = GetEvent();

			// Transform key/mouse events into SaberEngine functionality events (eg. "w" -> "move forward")
			// NOTE: We may receive more than 1 of each type of event between calls to Update() from input with high
			// polling rates (e.g. mouse motion)

			core::EventManager::EventInfo transformedEvent;

			bool doBroadcastToSE = true;

			switch (eventInfo.m_eventKey)
			{
			case eventkey::KeyboardInputCaptureChange:
			{
				m_keyboardInputCaptured = std::get<bool>(eventInfo.m_data);
				if (m_keyboardInputCaptured)
				{
					InitializeKeyboardStates();
				}
				doBroadcastToSE = false;
			}
			break;
			case eventkey::MouseInputCaptureChange:
			{
				m_mouseInputCaptured = std::get<bool>(eventInfo.m_data);
				if (m_mouseInputCaptured)
				{
					InitializeMouseAxisStates();
				}
				doBroadcastToSE = false;
			}
			break;
			case eventkey::KeyEvent:
			{
				std::pair<uint32_t, bool> const& data = std::get<std::pair<uint32_t, bool>>(eventInfo.m_data);
				
				const definitions::SEKeycode keycode = platform::InputManager::ConvertToSEKeycode(data.first);
				const bool keystate = data.second;

				auto const& result = m_SEKeycodesToSEEventEnums.find(keycode);
				if (result != m_SEKeycodesToSEEventEnums.end())
				{
					const definitions::KeyboardInputButton key = result->second;

					m_keyboardInputButtonStates[key] = keystate && !m_keyboardInputCaptured;

					transformedEvent.m_data = keystate; // Always true...

					// Note: We only broadcast key presses (not releases)
					doBroadcastToSE = keystate;

					switch (key)
					{
					case definitions::KeyboardInputButton::InputButton_Forward:
					{
						transformedEvent.m_eventKey = eventkey::InputForward;
					}
					break;
					case definitions::KeyboardInputButton::InputButton_Backward:
					{
						transformedEvent.m_eventKey = eventkey::InputBackward;
					}
					break;
					case definitions::KeyboardInputButton::InputButton_Left:
					{
						transformedEvent.m_eventKey = eventkey::InputLeft;
					}
					break;
					case definitions::KeyboardInputButton::InputButton_Right:
					{
						transformedEvent.m_eventKey = eventkey::InputRight;
					}
					break;
					case definitions::KeyboardInputButton::InputButton_Up:
					{
						transformedEvent.m_eventKey = eventkey::InputUp;
					}
					break;
					case definitions::KeyboardInputButton::InputButton_Down:
					{
						transformedEvent.m_eventKey = eventkey::InputDown;
					}
					break;
					case definitions::KeyboardInputButton::InputButton_Sprint:
					{
						transformedEvent.m_eventKey = eventkey::InputSprint;
					}
					break;
					case definitions::KeyboardInputButton::InputButton_ToggleUIVisibility:
					{
						transformedEvent.m_eventKey = eventkey::ToggleUIVisibility;
					}
					break;
					case definitions::KeyboardInputButton::InputButton_Console:
					{
						transformedEvent.m_eventKey = eventkey::ToggleConsole;
					}
					break;
					case definitions::KeyboardInputButton::InputButton_VSync:
					{
						transformedEvent.m_eventKey = eventkey::ToggleVSync;
					}
					break;
					default:
						SEAssertF("Input has not been handled. Is there a case for it in this switch?");
						break;
					}
				}
				else
				{
					// Not a key we've got mapped to an input/function
					doBroadcastToSE = false;
				}
			} // end KeyEvent
			break;
			case eventkey::MouseMotionEvent:
			{
				// Unpack the mouse data:
				std::pair<int32_t, int32_t> const& data = std::get<std::pair<int32_t, int32_t>>(eventInfo.m_data);
				
				m_mouseAxisStates[definitions::Input_MouseX] += static_cast<float>(data.first) * !m_mouseInputCaptured;
				m_mouseAxisStates[definitions::Input_MouseY] += static_cast<float>(data.second) * !m_mouseInputCaptured;
			}
			break;
			case eventkey::MouseButtonEvent:
			{
				std::pair<uint32_t, bool> const& data = std::get<std::pair<uint32_t, bool>>(eventInfo.m_data);

				const bool buttonState = data.second;
				switch (data.first)
				{
				case 0: // Left
				{
					m_mouseButtonStates[definitions::InputMouse_Left] = buttonState && !m_mouseInputCaptured;

					transformedEvent.m_eventKey = eventkey::InputMouseLeft;
					transformedEvent.m_data = buttonState;
				}
				break;
				case 1: // Middle
				{
					m_mouseButtonStates[definitions::InputMouse_Middle] = buttonState && !m_mouseInputCaptured;
					
					transformedEvent.m_eventKey = eventkey::InputMouseMiddle;
					transformedEvent.m_data = buttonState;
				}
				break;
				case 2: // Right
				{
					m_mouseButtonStates[definitions::InputMouse_Right] = buttonState && !m_mouseInputCaptured;
					
					transformedEvent.m_eventKey = eventkey::InputMouseRight;
					transformedEvent.m_data = buttonState;
				}
				break;
				default:
					SEAssertF("Invalid mouse button");
				}
			}
			break;
			case eventkey::MouseWheelEvent:
			{
				doBroadcastToSE = !m_mouseInputCaptured;
				if (doBroadcastToSE)
				{
					// Pass on the data set in EventManager_Win32.cpp
					transformedEvent.m_eventKey = eventkey::MouseWheelEvent;
					transformedEvent.m_data = eventInfo.m_data;
				}
			}
			break;
			case eventkey::WindowFocusChanged:
			{
				// If we've lost focus, zero out any currently-pressed keys to prevent them getting stuck
				if (!std::get<bool>(transformedEvent.m_data))
				{
					InitializeKeyboardStates();
				}
				doBroadcastToSE = false;
			}
			break;
			default:
				SEAssertF("Invalid event type");
				break;
			}

			if (doBroadcastToSE)
			{
				core::EventManager::Get()->Notify(std::move(transformedEvent));
			}
		}		
	}


	void InputManager::LoadInputBindings()
	{
		for (size_t i = 0; i < definitions::KeyboardInputButton_Count; i++)
		{
			// Get the key actually assigned to the current named input button
			// eg. Get "w" from "InputButton_Forward"
			std::string const& keyAssignment = 
				core::Config::Get()->GetValueAsString(definitions::KeyboardInputButtonNames[i]);

			SEAssert(!keyAssignment.empty(),
				std::format("Button not found in {}. Did you forget to set one in Config::InitializeDefaultValues()?",
				core::configkeys::k_configFileName).c_str());

			const definitions::SEKeycode keycode = definitions::GetSEKeycodeFromName(keyAssignment);
			if (keycode != definitions::SEK_UNKNOWN)
			{
				// Build a map: SEKeycode -> SaberEngine keyboard input function
				m_SEKeycodesToSEEventEnums.emplace(keycode, static_cast<definitions::KeyboardInputButton>(i));
			}
			else
			{
				// We want to assert if we can, but even if we're in Release mode we want to log an error:
				std::string const& errorMessage = "Invalid key name: \"" + keyAssignment + "\", cannot find a matching "
					"SEKeycode. Note: Key names are (currently) case sensitive";
				SEAssertF(errorMessage.c_str());

				// TODO: Key names shouldn't be case sensitive
			}
		}
	}


	void InputManager::InitializeKeyboardStates()
	{
		memset(m_keyboardInputButtonStates, 0, sizeof(bool) * definitions::KeyboardInputButton_Count);
	}


	void InputManager::InitializeMouseAxisStates()
	{
		memset(m_mouseAxisStates, 0, sizeof(float) * definitions::MouseInputAxis_Count);
	}


	void InputManager::InitializeMouseButtonStates()
	{
		memset(m_mouseButtonStates, 0, sizeof(bool) * definitions::MouseInputButton_Count);
	}
}

