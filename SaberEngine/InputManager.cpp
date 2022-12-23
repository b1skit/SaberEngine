#include <SDL.h>

#include "backends/imgui_impl_sdl.h"

#include "InputManager.h"
#include "Config.h"
#include "DebugConfiguration.h"
#include "EventManager.h"

using en::Config;
using en::EventManager;
using std::string;
using std::make_shared;


namespace
{
	void AddKeyEventToImGui(ImGuiIO& io, uint32_t sdlScancode, bool keystate)
	{		
		// Keyboard sections: left to right, row-by-row
		switch (sdlScancode)
		{
		case SDL_SCANCODE_ESCAPE: io.AddKeyEvent(ImGuiKey_Escape, keystate); break;
		case SDL_SCANCODE_F1: io.AddKeyEvent(ImGuiKey_F1, keystate); break;
		case SDL_SCANCODE_F2: io.AddKeyEvent(ImGuiKey_F2, keystate); break;
		case SDL_SCANCODE_F3: io.AddKeyEvent(ImGuiKey_F3, keystate); break;
		case SDL_SCANCODE_F4: io.AddKeyEvent(ImGuiKey_F4, keystate); break;
		case SDL_SCANCODE_F5: io.AddKeyEvent(ImGuiKey_F5, keystate); break;
		case SDL_SCANCODE_F6: io.AddKeyEvent(ImGuiKey_F6, keystate); break;
		case SDL_SCANCODE_F7: io.AddKeyEvent(ImGuiKey_F7, keystate); break;
		case SDL_SCANCODE_F8: io.AddKeyEvent(ImGuiKey_F8, keystate); break;
		case SDL_SCANCODE_F9: io.AddKeyEvent(ImGuiKey_F9, keystate); break;
		case SDL_SCANCODE_F10: io.AddKeyEvent(ImGuiKey_F10, keystate); break;
		case SDL_SCANCODE_F11: io.AddKeyEvent(ImGuiKey_F11, keystate); break;
		case SDL_SCANCODE_F12: io.AddKeyEvent(ImGuiKey_F12, keystate); break;

		case SDL_SCANCODE_GRAVE: io.AddKeyEvent(ImGuiKey_GraveAccent, keystate); break;
		case SDL_SCANCODE_0: io.AddKeyEvent(ImGuiKey_0, keystate); break;
		case SDL_SCANCODE_1: io.AddKeyEvent(ImGuiKey_1, keystate); break;
		case SDL_SCANCODE_2: io.AddKeyEvent(ImGuiKey_2, keystate); break;
		case SDL_SCANCODE_3: io.AddKeyEvent(ImGuiKey_3, keystate); break;
		case SDL_SCANCODE_4: io.AddKeyEvent(ImGuiKey_4, keystate); break;
		case SDL_SCANCODE_5: io.AddKeyEvent(ImGuiKey_5, keystate); break;
		case SDL_SCANCODE_6: io.AddKeyEvent(ImGuiKey_6, keystate); break;
		case SDL_SCANCODE_7: io.AddKeyEvent(ImGuiKey_7, keystate); break;
		case SDL_SCANCODE_8: io.AddKeyEvent(ImGuiKey_8, keystate); break;
		case SDL_SCANCODE_9: io.AddKeyEvent(ImGuiKey_9, keystate); break;
		case SDL_SCANCODE_MINUS: io.AddKeyEvent(ImGuiKey_Minus, keystate); break;
		case SDL_SCANCODE_EQUALS: io.AddKeyEvent(ImGuiKey_Equal, keystate); break;
		case SDL_SCANCODE_BACKSPACE: io.AddKeyEvent(ImGuiKey_Backspace, keystate); break;

		case SDL_SCANCODE_TAB: io.AddKeyEvent(ImGuiKey_Tab, keystate); break;
		case SDL_SCANCODE_Q: io.AddKeyEvent(ImGuiKey_Q, keystate); break;
		case SDL_SCANCODE_W: io.AddKeyEvent(ImGuiKey_W, keystate); break;
		case SDL_SCANCODE_E: io.AddKeyEvent(ImGuiKey_E, keystate); break;
		case SDL_SCANCODE_R: io.AddKeyEvent(ImGuiKey_R, keystate); break;
		case SDL_SCANCODE_T: io.AddKeyEvent(ImGuiKey_T, keystate); break;
		case SDL_SCANCODE_Y: io.AddKeyEvent(ImGuiKey_Y, keystate); break;
		case SDL_SCANCODE_U: io.AddKeyEvent(ImGuiKey_U, keystate); break;
		case SDL_SCANCODE_I: io.AddKeyEvent(ImGuiKey_I, keystate); break;
		case SDL_SCANCODE_O: io.AddKeyEvent(ImGuiKey_O, keystate); break;
		case SDL_SCANCODE_P: io.AddKeyEvent(ImGuiKey_P, keystate); break;
		case SDL_SCANCODE_LEFTBRACKET: io.AddKeyEvent(ImGuiKey_LeftBracket, keystate); break;
		case SDL_SCANCODE_RIGHTBRACKET: io.AddKeyEvent(ImGuiKey_RightBracket, keystate); break;
		case SDL_SCANCODE_BACKSLASH: io.AddKeyEvent(ImGuiKey_Backslash, keystate); break;

		case SDL_SCANCODE_CAPSLOCK: io.AddKeyEvent(ImGuiKey_CapsLock, keystate); break;
		case SDL_SCANCODE_A: io.AddKeyEvent(ImGuiKey_A, keystate); break;
		case SDL_SCANCODE_S: io.AddKeyEvent(ImGuiKey_S, keystate); break;
		case SDL_SCANCODE_D: io.AddKeyEvent(ImGuiKey_D, keystate); break;
		case SDL_SCANCODE_F: io.AddKeyEvent(ImGuiKey_F, keystate); break;
		case SDL_SCANCODE_G: io.AddKeyEvent(ImGuiKey_G, keystate); break;
		case SDL_SCANCODE_H: io.AddKeyEvent(ImGuiKey_H, keystate); break;
		case SDL_SCANCODE_J: io.AddKeyEvent(ImGuiKey_J, keystate); break;
		case SDL_SCANCODE_K: io.AddKeyEvent(ImGuiKey_K, keystate); break;
		case SDL_SCANCODE_L: io.AddKeyEvent(ImGuiKey_L, keystate); break;
		case SDL_SCANCODE_SEMICOLON: io.AddKeyEvent(ImGuiKey_Semicolon, keystate); break;
		case SDL_SCANCODE_APOSTROPHE: io.AddKeyEvent(ImGuiKey_Apostrophe, keystate); break;
		case SDL_SCANCODE_RETURN: io.AddKeyEvent(ImGuiKey_Enter, keystate); break;

		case SDL_SCANCODE_LSHIFT: io.AddKeyEvent(ImGuiKey_LeftShift, keystate); break;
		case SDL_SCANCODE_Z: io.AddKeyEvent(ImGuiKey_Z, keystate); break;
		case SDL_SCANCODE_X: io.AddKeyEvent(ImGuiKey_X, keystate); break;
		case SDL_SCANCODE_C: io.AddKeyEvent(ImGuiKey_C, keystate); break;
		case SDL_SCANCODE_V: io.AddKeyEvent(ImGuiKey_V, keystate); break;
		case SDL_SCANCODE_B: io.AddKeyEvent(ImGuiKey_B, keystate); break;
		case SDL_SCANCODE_N: io.AddKeyEvent(ImGuiKey_N, keystate); break;
		case SDL_SCANCODE_M: io.AddKeyEvent(ImGuiKey_M, keystate); break;
		case SDL_SCANCODE_COMMA: io.AddKeyEvent(ImGuiKey_Comma, keystate); break;
		case SDL_SCANCODE_PERIOD: io.AddKeyEvent(ImGuiKey_Period, keystate); break;
		case SDL_SCANCODE_SLASH: io.AddKeyEvent(ImGuiKey_Slash, keystate); break;
		case SDL_SCANCODE_RSHIFT: io.AddKeyEvent(ImGuiKey_RightShift, keystate); break;

		case SDL_SCANCODE_LCTRL: io.AddKeyEvent(ImGuiKey_LeftCtrl, keystate); break;
		case SDL_SCANCODE_APPLICATION: io.AddKeyEvent(ImGuiKey_Menu, keystate); break; // ?
		case SDL_SCANCODE_LALT: io.AddKeyEvent(ImGuiKey_LeftAlt, keystate); break;
		case SDL_SCANCODE_SPACE: io.AddKeyEvent(ImGuiKey_Space, keystate); break;
		case SDL_SCANCODE_RALT: io.AddKeyEvent(ImGuiKey_RightAlt, keystate); break;
		case SDL_SCANCODE_RCTRL: io.AddKeyEvent(ImGuiKey_RightCtrl, keystate); break;

		case SDL_SCANCODE_PRINTSCREEN: io.AddKeyEvent(ImGuiKey_PrintScreen, keystate); break;
		case SDL_SCANCODE_SCROLLLOCK: io.AddKeyEvent(ImGuiKey_ScrollLock, keystate); break;
		case SDL_SCANCODE_PAUSE: io.AddKeyEvent(ImGuiKey_Pause, keystate); break;

		case SDL_SCANCODE_INSERT: io.AddKeyEvent(ImGuiKey_Insert, keystate); break;
		case SDL_SCANCODE_HOME: io.AddKeyEvent(ImGuiKey_Home, keystate); break;
		case SDL_SCANCODE_PAGEUP: io.AddKeyEvent(ImGuiKey_PageUp, keystate); break;

		case SDL_SCANCODE_DELETE: io.AddKeyEvent(ImGuiKey_Delete, keystate); break;
		case SDL_SCANCODE_END: io.AddKeyEvent(ImGuiKey_End, keystate); break;
		case SDL_SCANCODE_PAGEDOWN: io.AddKeyEvent(ImGuiKey_PageDown, keystate); break;
		
		case SDL_SCANCODE_UP: io.AddKeyEvent(ImGuiKey_UpArrow, keystate); break;
		case SDL_SCANCODE_DOWN: io.AddKeyEvent(ImGuiKey_DownArrow, keystate); break;
		case SDL_SCANCODE_LEFT: io.AddKeyEvent(ImGuiKey_LeftArrow, keystate); break;
		case SDL_SCANCODE_RIGHT: io.AddKeyEvent(ImGuiKey_RightArrow, keystate);	break;

		case SDL_SCANCODE_NUMLOCKCLEAR: io.AddKeyEvent(ImGuiKey_NumLock, keystate);	break;
		// Note: No scancode for "*" or "+"
 
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


	float InputManager::GetMouseAxisInput(en::MouseInputAxis axis)
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

		m_mouseButtonStates[en::InputMouse_Left] = false;
		m_mouseButtonStates[en::InputMouse_Right] = false;

		HandleEvents();
		
		// Handle the console toggle key: Enables/disables locking the mouse to the window and hiding the pointer
		if (m_consoleTriggered != m_prevConsoleTriggeredState)
		{
			m_prevConsoleTriggeredState = m_consoleTriggered;
			SDL_SetRelativeMouseMode((SDL_bool)!m_consoleTriggered); // True hides the mouse and locks it to the window
		}
	}


	void InputManager::HandleEvents()
	{
		ImGuiIO& io = ImGui::GetIO();
		const bool imguiWantsToCaptureMouse = io.WantCaptureMouse;
		const bool imguiWantsToCaptureKeyboard = io.WantCaptureKeyboard;

		while (HasEvents())
		{
			en::EventManager::EventInfo eventInfo = GetEvent();

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
				const uint32_t sdlScancode = eventInfo.m_data0.m_dataUI;
				const bool keystate = eventInfo.m_data1.m_dataB;

				AddKeyEventToImGui(io, sdlScancode, keystate);

				doBroadcast = !io.WantCaptureKeyboard && !io.WantTextInput;
				if (doBroadcast)
				{
					auto const& result = m_SDLScancodsToSaberEngineEventEnums.find(sdlScancode);
					if (result != m_SDLScancodsToSaberEngineEventEnums.end())
					{
						const en::KeyboardInputButton key = result->second;

						m_keyboardInputButtonStates[key] = keystate;

						transformedEvent.m_data0.m_dataB = keystate;

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
						case KeyboardInputButton::InputButton_Quit:
						{
							transformedEvent.m_type = EventManager::EventType::EngineQuit;
						}
						break;
						default:
							SEAssertF("Invalid scancode");
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
					SEAssertF("TODO: Support middle mouse");
				}
				break;
				case 2: // Right
				{
					io.AddMouseButtonEvent(ImGuiMouseButton_Right, buttonState);
					if (imguiWantsToCaptureMouse)
					{
						doBroadcast = true;
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
				io.AddMouseWheelEvent(eventInfo.m_data0.m_dataF, eventInfo.m_data1.m_dataF);
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
			const string configButtonName = Config::Get()->GetValueAsString(en::KeyboardInputButtonNames[i]);

			SEAssert("Button not found in config.cfg. Did you forget to set one in Config::InitializeDefaultValues()?", 
				!configButtonName.empty());

			// Note: For now, we use SDL_Scancodes for all button presses.
			// Scancode = Location of a press. Best suited for layout-dependent keys (eg. WASD)
			// Keycode = Meaning of a press, with respect to the current keyboard layout (eg. qwerty vs azerty). Best
			//			suited for character-dependent keys (eg. Press "I" for inventory)
			// More info here:
			// https://stackoverflow.com/questions/56915258/difference-between-sdl-scancode-and-sdl-keycode

			SDL_Scancode scancode = SDL_GetScancodeFromName(configButtonName.c_str());
			if (scancode != SDL_SCANCODE_UNKNOWN)
			{
				m_SDLScancodsToSaberEngineEventEnums.insert(
					{ (uint32_t)scancode, static_cast<en::KeyboardInputButton>(i) });
			}
			else
			{
				// We want to assert if we can, but even if we're in Release mode we want to log an error:
				const string errorMessage = "Invalid key name: \"" + configButtonName + "\", cannot find a matching "
					"SDL scancode. Key names are case sensitive, see the \"Key Name\" column on this page for exact "
					"values: \nhttps://wiki.libsdl.org/SDL_Scancode";
				LOG_ERROR(errorMessage.c_str());
				SEAssertF(errorMessage.c_str());
			}
		}
	}
}

