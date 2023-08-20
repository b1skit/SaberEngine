// © 2022 Adam Badke. All rights reserved.
#include "InputManager.h"
#include "Config.h"
#include "DebugConfiguration.h"
#include "EventManager.h"
#include "CoreEngine.h"
#include "InputManager_Platform.h"

using en::Config;
using en::EventManager;
using std::string;
using std::make_shared;


namespace
{
	void AddKeyEventToImGui(ImGuiIO& io, en::SEKeycode keycode, bool keystate)
	{		
		// Keyboard sections: left to right, row-by-row
		switch (keycode)
		{
		case en::SEK_ESCAPE: io.AddKeyEvent(ImGuiKey_Escape, keystate); break;
		case en::SEK_F1: io.AddKeyEvent(ImGuiKey_F1, keystate); break;
		case en::SEK_F2: io.AddKeyEvent(ImGuiKey_F2, keystate); break;
		case en::SEK_F3: io.AddKeyEvent(ImGuiKey_F3, keystate); break;
		case en::SEK_F4: io.AddKeyEvent(ImGuiKey_F4, keystate); break;
		case en::SEK_F5: io.AddKeyEvent(ImGuiKey_F5, keystate); break;
		case en::SEK_F6: io.AddKeyEvent(ImGuiKey_F6, keystate); break;
		case en::SEK_F7: io.AddKeyEvent(ImGuiKey_F7, keystate); break;
		case en::SEK_F8: io.AddKeyEvent(ImGuiKey_F8, keystate); break;
		case en::SEK_F9: io.AddKeyEvent(ImGuiKey_F9, keystate); break;
		case en::SEK_F10: io.AddKeyEvent(ImGuiKey_F10, keystate); break;
		case en::SEK_F11: io.AddKeyEvent(ImGuiKey_F11, keystate); break;
		case en::SEK_F12: io.AddKeyEvent(ImGuiKey_F12, keystate); break;

		case en::SEK_GRAVE: io.AddKeyEvent(ImGuiKey_GraveAccent, keystate); break;
		case en::SEK_0: io.AddKeyEvent(ImGuiKey_0, keystate); break;
		case en::SEK_1: io.AddKeyEvent(ImGuiKey_1, keystate); break;
		case en::SEK_2: io.AddKeyEvent(ImGuiKey_2, keystate); break;
		case en::SEK_3: io.AddKeyEvent(ImGuiKey_3, keystate); break;
		case en::SEK_4: io.AddKeyEvent(ImGuiKey_4, keystate); break;
		case en::SEK_5: io.AddKeyEvent(ImGuiKey_5, keystate); break;
		case en::SEK_6: io.AddKeyEvent(ImGuiKey_6, keystate); break;
		case en::SEK_7: io.AddKeyEvent(ImGuiKey_7, keystate); break;
		case en::SEK_8: io.AddKeyEvent(ImGuiKey_8, keystate); break;
		case en::SEK_9: io.AddKeyEvent(ImGuiKey_9, keystate); break;
		case en::SEK_MINUS: io.AddKeyEvent(ImGuiKey_Minus, keystate); break;
		case en::SEK_EQUALS: io.AddKeyEvent(ImGuiKey_Equal, keystate); break;
		case en::SEK_BACKSPACE: io.AddKeyEvent(ImGuiKey_Backspace, keystate); break;

		case en::SEK_TAB: io.AddKeyEvent(ImGuiKey_Tab, keystate); break;
		case en::SEK_Q: io.AddKeyEvent(ImGuiKey_Q, keystate); break;
		case en::SEK_W: io.AddKeyEvent(ImGuiKey_W, keystate); break;
		case en::SEK_E: io.AddKeyEvent(ImGuiKey_E, keystate); break;
		case en::SEK_R: io.AddKeyEvent(ImGuiKey_R, keystate); break;
		case en::SEK_T: io.AddKeyEvent(ImGuiKey_T, keystate); break;
		case en::SEK_Y: io.AddKeyEvent(ImGuiKey_Y, keystate); break;
		case en::SEK_U: io.AddKeyEvent(ImGuiKey_U, keystate); break;
		case en::SEK_I: io.AddKeyEvent(ImGuiKey_I, keystate); break;
		case en::SEK_O: io.AddKeyEvent(ImGuiKey_O, keystate); break;
		case en::SEK_P: io.AddKeyEvent(ImGuiKey_P, keystate); break;
		case en::SEK_LEFTBRACKET: io.AddKeyEvent(ImGuiKey_LeftBracket, keystate); break;
		case en::SEK_RIGHTBRACKET: io.AddKeyEvent(ImGuiKey_RightBracket, keystate); break;
		case en::SEK_BACKSLASH: io.AddKeyEvent(ImGuiKey_Backslash, keystate); break;

		case en::SEK_CAPSLOCK: io.AddKeyEvent(ImGuiKey_CapsLock, keystate); break;
		case en::SEK_A: io.AddKeyEvent(ImGuiKey_A, keystate); break;
		case en::SEK_S: io.AddKeyEvent(ImGuiKey_S, keystate); break;
		case en::SEK_D: io.AddKeyEvent(ImGuiKey_D, keystate); break;
		case en::SEK_F: io.AddKeyEvent(ImGuiKey_F, keystate); break;
		case en::SEK_G: io.AddKeyEvent(ImGuiKey_G, keystate); break;
		case en::SEK_H: io.AddKeyEvent(ImGuiKey_H, keystate); break;
		case en::SEK_J: io.AddKeyEvent(ImGuiKey_J, keystate); break;
		case en::SEK_K: io.AddKeyEvent(ImGuiKey_K, keystate); break;
		case en::SEK_L: io.AddKeyEvent(ImGuiKey_L, keystate); break;
		case en::SEK_SEMICOLON: io.AddKeyEvent(ImGuiKey_Semicolon, keystate); break;
		case en::SEK_APOSTROPHE: io.AddKeyEvent(ImGuiKey_Apostrophe, keystate); break;
		case en::SEK_RETURN: io.AddKeyEvent(ImGuiKey_Enter, keystate); break;

		case en::SEK_LSHIFT: io.AddKeyEvent(ImGuiKey_LeftShift, keystate); break;
		case en::SEK_Z: io.AddKeyEvent(ImGuiKey_Z, keystate); break;
		case en::SEK_X: io.AddKeyEvent(ImGuiKey_X, keystate); break;
		case en::SEK_C: io.AddKeyEvent(ImGuiKey_C, keystate); break;
		case en::SEK_V: io.AddKeyEvent(ImGuiKey_V, keystate); break;
		case en::SEK_B: io.AddKeyEvent(ImGuiKey_B, keystate); break;
		case en::SEK_N: io.AddKeyEvent(ImGuiKey_N, keystate); break;
		case en::SEK_M: io.AddKeyEvent(ImGuiKey_M, keystate); break;
		case en::SEK_COMMA: io.AddKeyEvent(ImGuiKey_Comma, keystate); break;
		case en::SEK_PERIOD: io.AddKeyEvent(ImGuiKey_Period, keystate); break;
		case en::SEK_SLASH: io.AddKeyEvent(ImGuiKey_Slash, keystate); break;
		case en::SEK_RSHIFT: io.AddKeyEvent(ImGuiKey_RightShift, keystate); break;

		case en::SEK_LCTRL: io.AddKeyEvent(ImGuiKey_LeftCtrl, keystate); break;
		case en::SEK_APPLICATION: io.AddKeyEvent(ImGuiKey_Menu, keystate); break; // ?
		case en::SEK_LALT: io.AddKeyEvent(ImGuiKey_LeftAlt, keystate); break;
		case en::SEK_SPACE: io.AddKeyEvent(ImGuiKey_Space, keystate); break;
		case en::SEK_RALT: io.AddKeyEvent(ImGuiKey_RightAlt, keystate); break;
		case en::SEK_RCTRL: io.AddKeyEvent(ImGuiKey_RightCtrl, keystate); break;

		case en::SEK_PRINTSCREEN: io.AddKeyEvent(ImGuiKey_PrintScreen, keystate); break;
		case en::SEK_SCROLLLOCK: io.AddKeyEvent(ImGuiKey_ScrollLock, keystate); break;
		case en::SEK_PAUSE: io.AddKeyEvent(ImGuiKey_Pause, keystate); break;

		case en::SEK_INSERT: io.AddKeyEvent(ImGuiKey_Insert, keystate); break;
		case en::SEK_HOME: io.AddKeyEvent(ImGuiKey_Home, keystate); break;
		case en::SEK_PAGEUP: io.AddKeyEvent(ImGuiKey_PageUp, keystate); break;

		case en::SEK_DELETE: io.AddKeyEvent(ImGuiKey_Delete, keystate); break;
		case en::SEK_END: io.AddKeyEvent(ImGuiKey_End, keystate); break;
		case en::SEK_PAGEDOWN: io.AddKeyEvent(ImGuiKey_PageDown, keystate); break;
		
		case en::SEK_UP: io.AddKeyEvent(ImGuiKey_UpArrow, keystate); break;
		case en::SEK_DOWN: io.AddKeyEvent(ImGuiKey_DownArrow, keystate); break;
		case en::SEK_LEFT: io.AddKeyEvent(ImGuiKey_LeftArrow, keystate); break;
		case en::SEK_RIGHT: io.AddKeyEvent(ImGuiKey_RightArrow, keystate);	break;

		case en::SEK_NUMLOCK: io.AddKeyEvent(ImGuiKey_NumLock, keystate);	break;
 
		default: break; // Do nothing
		}
	}
}


namespace en
{
	// Static members:
	bool InputManager::m_keyboardInputButtonStates[en::KeyboardInputButton_Count];
	bool InputManager::m_mouseButtonStates[en::MouseInputButton_Count];
	float InputManager::m_mouseAxisStates[en::MouseInputAxis_Count];

	float InputManager::m_mousePitchSensitivity	= -0.00005f;
	float InputManager::m_mouseYawSensitivity	= -0.00005f;


	InputManager* InputManager::Get()
	{
		static std::unique_ptr<en::InputManager> instance = std::make_unique<en::InputManager>();
		return instance.get();
	}


	InputManager::InputManager()
		: m_consoleTriggered(false)
		, m_prevConsoleTriggeredState(false)
	{
		// Initialize keyboard keys:
		for (size_t i = 0; i < en::KeyboardInputButton_Count; i++)
		{
			m_keyboardInputButtonStates[i]	= false;
		}

		// Initialize mouse axes:
		for (int i = 0; i < en::MouseInputAxis_Count; i++)
		{
			m_mouseAxisStates[i] = 0.0f;
		}
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

		InputManager::m_mousePitchSensitivity = Config::Get()->GetValue<float>("mousePitchSensitivity") * -1.0f;
		InputManager::m_mouseYawSensitivity	= Config::Get()->GetValue<float>("mouseYawSensitivity") * -1.0f;

		// Event subscriptions:
		EventManager::Get()->Subscribe(EventManager::KeyEvent, this);
		EventManager::Get()->Subscribe(EventManager::TextInputEvent, this);
		EventManager::Get()->Subscribe(EventManager::MouseMotionEvent, this);
		EventManager::Get()->Subscribe(EventManager::MouseButtonEvent, this);
		EventManager::Get()->Subscribe(EventManager::MouseWheelEvent, this);

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
		
		// Handle the console toggle key: Enables/disables locking the mouse to the window and hiding the pointer
		if (m_consoleTriggered != m_prevConsoleTriggeredState)
		{
			m_prevConsoleTriggeredState = m_consoleTriggered;

			// If true, hide the mouse and lock it to the window
			const bool captureMouse = !m_consoleTriggered;
			en::CoreEngine::Get()->GetWindow()->SetRelativeMouseMode(captureMouse);

			// Disable ImGui mouse listening if the console is not active: Prevents UI elements
			// flashing as the hidden mouse cursor passes by
			ImGuiIO& io = ImGui::GetIO();
			if (m_consoleTriggered)
			{
				io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
			}
			else
			{
				io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
			}
		}
	}


	void InputManager::HandleEvents()
	{
		ImGuiIO& io = ImGui::GetIO();
		const bool imguiWantsToCaptureMouse = io.WantCaptureMouse;
		const bool imguiWantsToCaptureKeyboard = io.WantCaptureKeyboard;

		while (HasEvents())
		{
			en::EventManager::EventInfo const& eventInfo = GetEvent();

			// Transform key/mouse events into SaberEngine functionality events (eg. "w" -> "move forward")
			// NOTE: We may receive more than 1 of each type of event between calls to Update() from input with high
			// polling rates (e.g. mouse motion)

			EventManager::EventInfo transformedEvent;

			bool doBroadcast = true;

			switch (eventInfo.m_type)
			{
			case EventManager::TextInputEvent:
			{
				io.AddInputCharacter(eventInfo.m_data0.m_dataC);
				doBroadcast = false;
			}
			break;
			case EventManager::KeyEvent:
			{
				const SEKeycode keycode = platform::InputManager::ConvertToSEKeycode(eventInfo.m_data0.m_dataUI);
				const bool keystate = eventInfo.m_data1.m_dataB;

				AddKeyEventToImGui(io, keycode, keystate);

				doBroadcast = !io.WantCaptureKeyboard && !io.WantTextInput;
				if (doBroadcast)
				{
					auto const& result = m_SEKeycodesToSEEventEnums.find(keycode);
					if (result != m_SEKeycodesToSEEventEnums.end())
					{
						const en::KeyboardInputButton key = result->second;

						m_keyboardInputButtonStates[key] = keystate;

						transformedEvent.m_data0.m_dataB = keystate; // Always true...

						// Note: We only broadcast key presses (not releases)
						doBroadcast = keystate;

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
							// The InputManager must broadcast the transformed console toggle event, as well as react to it
							transformedEvent.m_type = EventManager::EventType::InputToggleConsole;

							// Toggle the mouse locking for the console display when the button is pressed down only
							if (keystate == true)
							{
								m_consoleTriggered = !m_consoleTriggered;
							}
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
						doBroadcast = false; 
					}
				}
			} // end KeyEvent
			break;
			case EventManager::MouseMotionEvent:
			{
				// Unpack the mouse data:
				m_mouseAxisStates[en::Input_MouseX] += static_cast<float>(eventInfo.m_data0.m_dataI) * m_mousePitchSensitivity;
				m_mouseAxisStates[en::Input_MouseY] += static_cast<float>(eventInfo.m_data1.m_dataI) * m_mouseYawSensitivity;
				doBroadcast = false;
			}
			break;
			case EventManager::MouseButtonEvent:
			{
				const bool buttonState = eventInfo.m_data1.m_dataB;
				switch (eventInfo.m_data0.m_dataUI)
				{
				case 0: // Left
				{
					io.AddMouseButtonEvent(ImGuiMouseButton_Left, buttonState);
					if (imguiWantsToCaptureMouse)
					{
						doBroadcast = false;
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
					io.AddMouseButtonEvent(ImGuiMouseButton_Middle, buttonState);
					if (imguiWantsToCaptureMouse)
					{
						doBroadcast = false;
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
					io.AddMouseButtonEvent(ImGuiMouseButton_Right, buttonState);
					if (imguiWantsToCaptureMouse)
					{
						doBroadcast = false;
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
			case EventManager::MouseWheelEvent:
			{
				// Broadcast to ImGui:
				io.AddMouseWheelEvent(
					static_cast<float>(eventInfo.m_data0.m_dataI), static_cast<float>(eventInfo.m_data1.m_dataI));
				doBroadcast = false;
			}
			break;
			default:
				SEAssertF("Invalid event type");
				break;
			}

			if (doBroadcast)
			{
				SEAssert("Event type is not initialized", transformedEvent.m_type != EventManager::Uninitialized);
				EventManager::Get()->Notify(transformedEvent);
			}
		}		
	}


	void InputManager::LoadInputBindings()
	{
		for (size_t i = 0; i < en::KeyboardInputButton_Count; i++)
		{
			// Get the key actually assigned to the current named input button
			// eg. Get "w" from "InputButton_Forward"
			const string keyAssignment = Config::Get()->GetValueAsString(en::KeyboardInputButtonNames[i]);

			SEAssert("Button not found in config.cfg. Did you forget to set one in Config::InitializeDefaultValues()?", 
				!keyAssignment.empty());

			SEKeycode keycode = GetSEKeycodeFromName(keyAssignment);
			if (keycode != SEK_UNKNOWN)
			{
				// Build a map: SEKeycode -> SaberEngine keyboard input function
				m_SEKeycodesToSEEventEnums.insert({keycode, static_cast<en::KeyboardInputButton>(i) });
			}
			else
			{
				// We want to assert if we can, but even if we're in Release mode we want to log an error:
				const string errorMessage = "Invalid key name: \"" + keyAssignment + "\", cannot find a matching "
					"SEKeycode. Note: Key names are (currently) case sensitive";
				LOG_ERROR(errorMessage.c_str());
				SEAssertF(errorMessage.c_str());

				// TODO: Key names shouldn't be case sensitive
			}
		}
	}
}

