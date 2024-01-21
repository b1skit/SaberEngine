// � 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Config.h"
#include "EventManager.h"
#include "InputManager.h"
#include "InputManager_Platform.h"


namespace en
{
	// Static members:
	bool InputManager::m_keyboardInputButtonStates[en::KeyboardInputButton_Count];
	bool InputManager::m_mouseButtonStates[en::MouseInputButton_Count];
	float InputManager::m_mouseAxisStates[en::MouseInputAxis_Count];


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
		InitializeMouseStates();
	}


	bool const& InputManager::GetKeyboardInputState(en::KeyboardInputButton key)
	{
		return m_keyboardInputButtonStates[key];
	}


	bool const& InputManager::GetMouseInputState(en::MouseInputButton button)
	{
		return m_mouseButtonStates[button];
	}


	float InputManager::GetRelativeMouseInput(en::MouseInputAxis axis)
	{
		return m_mouseAxisStates[axis];
	}
	

	void InputManager::Startup()
	{
		LOG("InputManager starting...");

		LoadInputBindings();

		// Event subscriptions:
		EventManager::Get()->Subscribe(EventManager::KeyEvent, this);
		EventManager::Get()->Subscribe(EventManager::MouseMotionEvent, this);
		EventManager::Get()->Subscribe(EventManager::MouseButtonEvent, this);
		EventManager::Get()->Subscribe(EventManager::MouseWheelEvent, this);
		EventManager::Get()->Subscribe(EventManager::WindowFocusChanged, this);
		EventManager::Get()->Subscribe(EventManager::KeyboardInputCaptureChange, this);
		EventManager::Get()->Subscribe(EventManager::MouseInputCaptureChange, this);

		platform::InputManager::Startup(*this);
	}


	void InputManager::Shutdown()
	{
		LOG("Input manager shutting down...");
	}


	void InputManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		// Prepare for the next around of input events fired by the EventManager
		m_mouseAxisStates[MouseInputAxis::Input_MouseX] = 0.f;
		m_mouseAxisStates[MouseInputAxis::Input_MouseY] = 0.f;

		for (size_t mButton = 0; mButton < MouseInputButton_Count; mButton++)
		{
			m_mouseButtonStates[mButton] = false;
		}

		HandleEvents();
	}


	void InputManager::HandleEvents()
	{
		while (HasEvents())
		{
			en::EventManager::EventInfo const& eventInfo = GetEvent();

			// Transform key/mouse events into SaberEngine functionality events (eg. "w" -> "move forward")
			// NOTE: We may receive more than 1 of each type of event between calls to Update() from input with high
			// polling rates (e.g. mouse motion)

			en::EventManager::EventInfo transformedEvent;

			bool doBroadcastToSE = true;

			switch (eventInfo.m_type)
			{
			case en::EventManager::KeyboardInputCaptureChange:
			{
				m_keyboardInputCaptured = eventInfo.m_data0.m_dataB;
				if (m_keyboardInputCaptured)
				{
					InitializeKeyboardStates();
				}
				doBroadcastToSE = false;
			}
			break;
			case en::EventManager::MouseInputCaptureChange:
			{
				m_mouseInputCaptured = eventInfo.m_data0.m_dataB;
				if (m_mouseInputCaptured)
				{
					InitializeMouseStates();
				}
				doBroadcastToSE = false;
			}
			break;
			case en::EventManager::KeyEvent:
			{
				const SEKeycode keycode = platform::InputManager::ConvertToSEKeycode(eventInfo.m_data0.m_dataUI);
				const bool keystate = eventInfo.m_data1.m_dataB;

				doBroadcastToSE = !m_keyboardInputCaptured;
				if (doBroadcastToSE)
				{
					auto const& result = m_SEKeycodesToSEEventEnums.find(keycode);
					if (result != m_SEKeycodesToSEEventEnums.end())
					{
						const en::KeyboardInputButton key = result->second;

						m_keyboardInputButtonStates[key] = keystate;

						transformedEvent.m_data0.m_dataB = keystate; // Always true...

						// Note: We only broadcast key presses (not releases)
						doBroadcastToSE = keystate;

						switch (key)
						{
						case KeyboardInputButton::InputButton_Forward:
						{
							transformedEvent.m_type = EventManager::EventType::InputForward;
						}
						break;
						case KeyboardInputButton::InputButton_Backward:
						{
							transformedEvent.m_type = EventManager::EventType::InputBackward;
						}
						break;
						case KeyboardInputButton::InputButton_Left:
						{
							transformedEvent.m_type = EventManager::EventType::InputLeft;
						}
						break;
						case KeyboardInputButton::InputButton_Right:
						{
							transformedEvent.m_type = EventManager::EventType::InputRight;
						}
						break;
						case KeyboardInputButton::InputButton_Up:
						{
							transformedEvent.m_type = EventManager::EventType::InputUp;
						}
						break;
						case KeyboardInputButton::InputButton_Down:
						{
							transformedEvent.m_type = EventManager::EventType::InputDown;
						}
						break;
						case KeyboardInputButton::InputButton_Sprint:
						{
							transformedEvent.m_type = EventManager::EventType::InputSprint;
						}
						break;
						case KeyboardInputButton::InputButton_Console:
						{
							transformedEvent.m_type = EventManager::EventType::InputToggleConsole;
						}
						break;
						case KeyboardInputButton::InputButton_VSync:
						{
							transformedEvent.m_type = EventManager::EventType::InputToggleVSync;							
						}
						break;
						case KeyboardInputButton::InputButton_Quit:
						{
							transformedEvent.m_type = EventManager::EventType::EngineQuit;
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
			case en::EventManager::MouseMotionEvent:
			{
				// Unpack the mouse data:
				m_mouseAxisStates[en::Input_MouseX] += static_cast<float>(eventInfo.m_data0.m_dataI);
				m_mouseAxisStates[en::Input_MouseY] += static_cast<float>(eventInfo.m_data1.m_dataI);
				doBroadcastToSE = false;
			}
			break;
			case en::EventManager::MouseButtonEvent:
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
						m_mouseButtonStates[en::InputMouse_Left] = buttonState;
						transformedEvent.m_type = EventManager::EventType::InputMouseLeft;
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
						m_mouseButtonStates[en::InputMouse_Middle] = buttonState;
						transformedEvent.m_type = EventManager::EventType::InputMouseMiddle;
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
						m_mouseButtonStates[en::InputMouse_Right] = buttonState;
						transformedEvent.m_type = EventManager::EventType::InputMouseRight;
						transformedEvent.m_data0.m_dataB = buttonState;
					}
				}
				break;
				default:
					SEAssertF("Invalid mouse button");
				}
			}
			break;
			case en::EventManager::MouseWheelEvent:
			{
				doBroadcastToSE = !m_mouseInputCaptured;
				if (doBroadcastToSE)
				{
					// Pass on the data set in EventManager_Win32.cpp
					transformedEvent.m_type = EventManager::EventType::MouseWheelEvent;
					transformedEvent.m_data0.m_dataI = eventInfo.m_data0.m_dataI;
					transformedEvent.m_data1.m_dataI = eventInfo.m_data1.m_dataI;
				}
			}
			break;
			case en::EventManager::WindowFocusChanged:
			{
				// If we've lost focus, zero out any currently-pressed keys to prevent them getting stuck				
				InitializeKeyboardStates();
				doBroadcastToSE = false;
			}
			break;
			default:
				SEAssertF("Invalid event type");
				break;
			}

			if (doBroadcastToSE)
			{
				en::EventManager::Get()->Notify(std::move(transformedEvent));
			}
		}		
	}


	void InputManager::LoadInputBindings()
	{
		for (size_t i = 0; i < en::KeyboardInputButton_Count; i++)
		{
			// Get the key actually assigned to the current named input button
			// eg. Get "w" from "InputButton_Forward"
			const std::string keyAssignment = Config::Get()->GetValueAsString(en::KeyboardInputButtonNames[i]);

			SEAssert(!keyAssignment.empty(),
				"Button not found in config.cfg. Did you forget to set one in Config::InitializeDefaultValues()?");

			SEKeycode keycode = GetSEKeycodeFromName(keyAssignment);
			if (keycode != SEK_UNKNOWN)
			{
				// Build a map: SEKeycode -> SaberEngine keyboard input function
				m_SEKeycodesToSEEventEnums.emplace(keycode, static_cast<en::KeyboardInputButton>(i));
			}
			else
			{
				// We want to assert if we can, but even if we're in Release mode we want to log an error:
				const std::string errorMessage = "Invalid key name: \"" + keyAssignment + "\", cannot find a matching "
					"SEKeycode. Note: Key names are (currently) case sensitive";
				LOG_ERROR(errorMessage.c_str());
				SEAssertF(errorMessage.c_str());

				// TODO: Key names shouldn't be case sensitive
			}
		}
	}


	void InputManager::InitializeKeyboardStates()
	{
		for (size_t i = 0; i < en::KeyboardInputButton_Count; i++)
		{
			m_keyboardInputButtonStates[i] = false;
		}
	}


	void InputManager::InitializeMouseStates()
	{
		for (int i = 0; i < en::MouseInputAxis_Count; i++)
		{
			m_mouseAxisStates[i] = 0.0f;
		}
	}
}

