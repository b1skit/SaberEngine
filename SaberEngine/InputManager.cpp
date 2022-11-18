#include <memory>

#include <SDL.h>

#include "imgui.h"
#include "backends/imgui_impl_sdl.h"

#include "InputManager.h"
#include "Config.h"
#include "DebugConfiguration.h"
#include "EventManager.h"

using en::Config;
using en::EventManager;
using std::string;
using std::make_shared;


namespace en
{
	// Static members:
	bool InputManager::m_keyboardInputButtonStates[en::KeyboardInputButton_Count];
	bool InputManager::m_mouseButtonStates[en::MouseInputButton_Count];
	float InputManager::m_mouseAxisStates[en::MouseInputAxis_Count];

	float InputManager::m_mousePitchSensitivity	= -0.00005f;
	float InputManager::m_mouseYawSensitivity	= -0.00005f;

	std::unique_ptr<InputManager> InputManager::m_instance = nullptr;
	InputManager* InputManager::Get()
	{
		if (m_instance == nullptr)
		{
			m_instance = std::make_unique<InputManager>();
		}
		return m_instance.get();
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
		EventManager::Get()->Subscribe(EventManager::MouseMotionEvent, this);
		EventManager::Get()->Subscribe(EventManager::MouseButtonEvent, this);
	}


	void InputManager::Shutdown()
	{
		LOG("Input manager shutting down...");
	}


	void InputManager::Update(const double stepTimeMs)
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

			bool eventIsBroadcastable = true;
			bool imguiCapturedEvent = false;

			switch (eventInfo.m_type)
			{
			case EventManager::KeyEvent:
			{
				const uint32_t sdlScancode = eventInfo.m_data0.m_dataUI;

				auto result = m_SDLScancodsToSaberEngineEventEnums.find(sdlScancode);
				if (result != m_SDLScancodsToSaberEngineEventEnums.end())
				{
					en::KeyboardInputButton key = result->second;
					const bool keystate = eventInfo.m_data1.m_dataB;

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
					eventIsBroadcastable = false;
				}
			} // End KeyEvent
			break;
			case EventManager::MouseMotionEvent:
			{
				// Unpack the mouse data:
				m_mouseAxisStates[en::Input_MouseX] += static_cast<float>(eventInfo.m_data0.m_dataI) * m_mousePitchSensitivity;
				m_mouseAxisStates[en::Input_MouseY] += static_cast<float>(eventInfo.m_data1.m_dataI) * m_mouseYawSensitivity;
				eventIsBroadcastable = false;
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
						imguiCapturedEvent = true;
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
						imguiCapturedEvent = true;
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
			default:
				SEAssertF("Invalid event type");
				break;
			}

			if (eventIsBroadcastable && !imguiCapturedEvent)
			{
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

