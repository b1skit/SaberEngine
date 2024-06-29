// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Config.h"
#include "EventManager.h"
#include "InputManager.h"
#include "InputManager_Platform.h"

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
		core::EventManager::Get()->Subscribe(core::EventManager::KeyEvent, this);
		core::EventManager::Get()->Subscribe(core::EventManager::MouseMotionEvent, this);
		core::EventManager::Get()->Subscribe(core::EventManager::MouseButtonEvent, this);
		core::EventManager::Get()->Subscribe(core::EventManager::MouseWheelEvent, this);
		core::EventManager::Get()->Subscribe(core::EventManager::WindowFocusChanged, this);
		core::EventManager::Get()->Subscribe(core::EventManager::KeyboardInputCaptureChange, this);
		core::EventManager::Get()->Subscribe(core::EventManager::MouseInputCaptureChange, this);

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

			switch (eventInfo.m_type)
			{
			case core::EventManager::KeyboardInputCaptureChange:
			{
				m_keyboardInputCaptured = eventInfo.m_data0.m_dataB;
				if (m_keyboardInputCaptured)
				{
					InitializeKeyboardStates();
				}
				doBroadcastToSE = false;
			}
			break;
			case core::EventManager::MouseInputCaptureChange:
			{
				m_mouseInputCaptured = eventInfo.m_data0.m_dataB;
				if (m_mouseInputCaptured)
				{
					InitializeMouseAxisStates();
				}
				doBroadcastToSE = false;
			}
			break;
			case core::EventManager::KeyEvent:
			{
				const definitions::SEKeycode keycode = platform::InputManager::ConvertToSEKeycode(eventInfo.m_data0.m_dataUI);
				const bool keystate = eventInfo.m_data1.m_dataB;

				doBroadcastToSE = !m_keyboardInputCaptured;
				if (doBroadcastToSE)
				{
					auto const& result = m_SEKeycodesToSEEventEnums.find(keycode);
					if (result != m_SEKeycodesToSEEventEnums.end())
					{
						const definitions::KeyboardInputButton key = result->second;

						m_keyboardInputButtonStates[key] = keystate;

						transformedEvent.m_data0.m_dataB = keystate; // Always true...

						// Note: We only broadcast key presses (not releases)
						doBroadcastToSE = keystate;

						switch (key)
						{
						case definitions::KeyboardInputButton::InputButton_Forward:
						{
							transformedEvent.m_type = core::EventManager::EventType::InputForward;
						}
						break;
						case definitions::KeyboardInputButton::InputButton_Backward:
						{
							transformedEvent.m_type = core::EventManager::EventType::InputBackward;
						}
						break;
						case definitions::KeyboardInputButton::InputButton_Left:
						{
							transformedEvent.m_type = core::EventManager::EventType::InputLeft;
						}
						break;
						case definitions::KeyboardInputButton::InputButton_Right:
						{
							transformedEvent.m_type = core::EventManager::EventType::InputRight;
						}
						break;
						case definitions::KeyboardInputButton::InputButton_Up:
						{
							transformedEvent.m_type = core::EventManager::EventType::InputUp;
						}
						break;
						case definitions::KeyboardInputButton::InputButton_Down:
						{
							transformedEvent.m_type = core::EventManager::EventType::InputDown;
						}
						break;
						case definitions::KeyboardInputButton::InputButton_Sprint:
						{
							transformedEvent.m_type = core::EventManager::EventType::InputSprint;
						}
						break;
						case definitions::KeyboardInputButton::InputButton_Console:
						{
							transformedEvent.m_type = core::EventManager::EventType::InputToggleConsole;
						}
						break;
						case definitions::KeyboardInputButton::InputButton_VSync:
						{
							transformedEvent.m_type = core::EventManager::EventType::InputToggleVSync;							
						}
						break;
						case definitions::KeyboardInputButton::InputButton_Quit:
						{
							transformedEvent.m_type = core::EventManager::EventType::EngineQuit;
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
				}
			} // end KeyEvent
			break;
			case core::EventManager::MouseMotionEvent:
			{
				// Unpack the mouse data:
				m_mouseAxisStates[definitions::Input_MouseX] += static_cast<float>(eventInfo.m_data0.m_dataI);
				m_mouseAxisStates[definitions::Input_MouseY] += static_cast<float>(eventInfo.m_data1.m_dataI);
				doBroadcastToSE = false;
			}
			break;
			case core::EventManager::MouseButtonEvent:
			{
				const bool buttonState = eventInfo.m_data1.m_dataB;
				switch (eventInfo.m_data0.m_dataUI)
				{
				case 0: // Left
				{
					if (m_mouseInputCaptured)
					{
						doBroadcastToSE = false;
					}
					else
					{
						m_mouseButtonStates[definitions::InputMouse_Left] = buttonState;
						transformedEvent.m_type = core::EventManager::EventType::InputMouseLeft;
						transformedEvent.m_data0.m_dataB = buttonState;
					}
				}
				break;
				case 1: // Middle
				{
					if (m_mouseInputCaptured)
					{
						doBroadcastToSE = false;
					}
					else
					{
						m_mouseButtonStates[definitions::InputMouse_Middle] = buttonState;
						transformedEvent.m_type = core::EventManager::EventType::InputMouseMiddle;
						transformedEvent.m_data0.m_dataB = buttonState;
					}
				}
				break;
				case 2: // Right
				{
					if (m_mouseInputCaptured)
					{
						doBroadcastToSE = false;
					}
					else
					{
						m_mouseButtonStates[definitions::InputMouse_Right] = buttonState;
						transformedEvent.m_type = core::EventManager::EventType::InputMouseRight;
						transformedEvent.m_data0.m_dataB = buttonState;
					}
				}
				break;
				default:
					SEAssertF("Invalid mouse button");
				}
			}
			break;
			case core::EventManager::MouseWheelEvent:
			{
				doBroadcastToSE = !m_mouseInputCaptured;
				if (doBroadcastToSE)
				{
					// Pass on the data set in EventManager_Win32.cpp
					transformedEvent.m_type = core::EventManager::EventType::MouseWheelEvent;
					transformedEvent.m_data0.m_dataI = eventInfo.m_data0.m_dataI;
					transformedEvent.m_data1.m_dataI = eventInfo.m_data1.m_dataI;
				}
			}
			break;
			case core::EventManager::WindowFocusChanged:
			{
				// If we've lost focus, zero out any currently-pressed keys to prevent them getting stuck
				if (!transformedEvent.m_data0.m_dataB)
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

