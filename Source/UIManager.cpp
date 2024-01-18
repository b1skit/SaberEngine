// © 2023 Adam Badke. All rights reserved.
#include "Config.h"
#include "CoreEngine.h"
#include "EntityManager.h"
#include "InputManager_Platform.h"
#include "KeyConfiguration.h"
#include "UIManager.h"
#include "LogManager.h"
#include "RenderManager.h"


namespace
{
	// Helper wrapper to reduce boilerplate
	void SendEvent(en::EventManager::EventType eventType)
	{
		en::EventManager::Get()->Notify(en::EventManager::EventInfo{
				.m_type = eventType,
				//.m_data0 = ,
				//.m_data1 = 
			});
	}


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

namespace fr
{
	UIManager* UIManager::Get()
	{
		static std::unique_ptr<fr::UIManager> instance = std::make_unique<fr::UIManager>();
		return instance.get();
	}


	UIManager::UIManager()
		: m_imguiMenuVisible(false)
		, m_prevImguiMenuVisible(false)
		, m_imguiWantsToCaptureKeyboard(false)
		, m_imguiWantsToCaptureMouse(false)
	{
	}


	void UIManager::Startup()
	{
		LOG("UI manager starting...");

		// Event subscriptions:
		en::EventManager::Get()->Subscribe(en::EventManager::EventType::InputToggleConsole, this);

		// Input events:
		en::EventManager::Get()->Subscribe(en::EventManager::EventType::TextInputEvent, this);
		en::EventManager::Get()->Subscribe(en::EventManager::EventType::KeyEvent, this);
		en::EventManager::Get()->Subscribe(en::EventManager::EventType::MouseMotionEvent, this);
		en::EventManager::Get()->Subscribe(en::EventManager::EventType::MouseButtonEvent, this);
		en::EventManager::Get()->Subscribe(en::EventManager::EventType::MouseWheelEvent, this);
	}


	void UIManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		HandleEvents();

		// ImGui visibility state has changed:
		if (m_imguiMenuVisible != m_prevImguiMenuVisible)
		{
			m_prevImguiMenuVisible = m_imguiMenuVisible;

			// If true, hide the mouse and lock it to the window
			const bool captureMouse = !m_imguiMenuVisible;
			en::CoreEngine::Get()->GetWindow()->SetRelativeMouseMode(captureMouse);

			// Disable ImGui mouse listening if the console is not active: Prevents UI elements
			// flashing as the hidden mouse cursor passes by
			{
				std::lock_guard<std::mutex> lock(re::RenderManager::Get()->GetGlobalImGuiMutex());

				ImGuiIO& io = ImGui::GetIO();
				if (m_imguiMenuVisible)
				{
					io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
				}
				else
				{
					io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
				}
			}
		}

		// Capture input, if necessary:
		if (m_imguiMenuVisible)
		{
			{
				bool currentImguiWantsToCaptureKeyboard = 0;
				bool currentImguiWantsToCaptureMouse = 0;
				{
					std::lock_guard<std::mutex> lock(re::RenderManager::Get()->GetGlobalImGuiMutex());
					ImGuiIO& io = ImGui::GetIO();
					currentImguiWantsToCaptureKeyboard = io.WantCaptureKeyboard;
					currentImguiWantsToCaptureMouse = io.WantCaptureMouse;
				}

				if (currentImguiWantsToCaptureKeyboard != m_imguiWantsToCaptureKeyboard)
				{
					m_imguiWantsToCaptureKeyboard = currentImguiWantsToCaptureKeyboard;

					en::EventManager::Get()->Notify(en::EventManager::EventInfo{
						.m_type = en::EventManager::EventType::KeyboardInputCaptureChange,
						.m_data0 = en::EventManager::EventData{ .m_dataB = m_imguiWantsToCaptureKeyboard },
						//.m_data1 = 
					});
				}

				if (currentImguiWantsToCaptureMouse != m_imguiWantsToCaptureMouse)
				{
					m_imguiWantsToCaptureMouse = currentImguiWantsToCaptureMouse;

					en::EventManager::Get()->Notify(en::EventManager::EventInfo{
						.m_type = en::EventManager::EventType::MouseInputCaptureChange,
						.m_data0 = en::EventManager::EventData{.m_dataB = m_imguiWantsToCaptureMouse },
						//.m_data1 = 
					});
				}
			}
		}

		SubmitImGuiRenderCommands();
	}


	void UIManager::Shutdown()
	{
		LOG("UI manager shutting down...");
		m_imguiMenuVisible = false;
	}


	void UIManager::HandleEvents()
	{
		while (HasEvents())
		{
			en::EventManager::EventInfo const& eventInfo = GetEvent();

			switch (eventInfo.m_type)
			{
			case en::EventManager::EventType::InputToggleConsole:
			{
				if (eventInfo.m_data0.m_dataB)
				{
					m_imguiMenuVisible = !m_imguiMenuVisible;
				}
			}
			break;
			case en::EventManager::EventType::TextInputEvent:
			{
				{
					std::lock_guard<std::mutex> lock(re::RenderManager::Get()->GetGlobalImGuiMutex());
					ImGuiIO& io = ImGui::GetIO();
					io.AddInputCharacter(eventInfo.m_data0.m_dataC);
				}
			}
			break;
			case en::EventManager::KeyEvent:
			{
				const en::SEKeycode keycode = platform::InputManager::ConvertToSEKeycode(eventInfo.m_data0.m_dataUI);
				const bool keystate = eventInfo.m_data1.m_dataB;

				if (m_imguiWantsToCaptureKeyboard)
				{
					std::lock_guard<std::mutex> lock(re::RenderManager::Get()->GetGlobalImGuiMutex());
					ImGuiIO& io = ImGui::GetIO();
					AddKeyEventToImGui(io, keycode, keystate);
				}
			}
			break;
			case en::EventManager::MouseButtonEvent:
			{
				const bool buttonState = eventInfo.m_data1.m_dataB;
				{
					std::lock_guard<std::mutex> lock(re::RenderManager::Get()->GetGlobalImGuiMutex());
					ImGuiIO& io = ImGui::GetIO();

					switch (eventInfo.m_data0.m_dataUI)
					{
					case 0: // Left
					{
						io.AddMouseButtonEvent(ImGuiMouseButton_Left, buttonState);
					}
					break;
					case 1: // Middle
					{
						io.AddMouseButtonEvent(ImGuiMouseButton_Middle, buttonState);
					}
					break;
					case 2: // Right
					{
						io.AddMouseButtonEvent(ImGuiMouseButton_Right, buttonState);
					}
					break;
					default:
						SEAssertF("Invalid mouse button");
					}
				}
			}
			break;
			case en::EventManager::MouseWheelEvent:
			{
				{
					std::lock_guard<std::mutex> lock(re::RenderManager::Get()->GetGlobalImGuiMutex());
					ImGuiIO& io = ImGui::GetIO();
					io.AddMouseWheelEvent(
						static_cast<float>(eventInfo.m_data0.m_dataI), static_cast<float>(eventInfo.m_data1.m_dataI));
				}
			}
			break;
			default:
				break;
			}
		}
	}


	void UIManager::SubmitImGuiRenderCommands()
	{
		// Importantly, this function does NOT modify any ImGui state. Instead, it submits commands to the render
		// manager, which will execute the updates on the render thread

		static bool s_showConsoleLog = false;
		static bool s_showEntityMgrDebug = false;
		static bool s_showTransformHierarchyDebug = false;
		static bool s_showEntityComponentDebug = false;
		static bool s_showRenderMgrDebug = false;
		static bool s_showRenderDataDebug = false;
		static bool s_showImguiDemo = false;

//#define FORCE_SHOW_IMGUI_DEMO
#if defined(_DEBUG) || defined(FORCE_SHOW_IMGUI_DEMO)
#define SHOW_IMGUI_DEMO_WINDOW
#endif

		// Early out if we can
		if (!m_imguiMenuVisible && 
			!s_showConsoleLog && 
			!s_showEntityMgrDebug && 
			!s_showTransformHierarchyDebug && 
			!s_showEntityComponentDebug &&
			!s_showRenderMgrDebug && 
			!s_showRenderDataDebug &&
			!s_showImguiDemo)
		{
			return;
		}

		static const int windowWidth = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowWidthKey);
		static const int windowHeight = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowHeightKey);

		static ImVec2 menuBarSize = { 0, 0 }; // Record the size of the menu bar so we can align things absolutely underneath it

		// Menu bar:
		auto ShowMenuBar = [&]()
			{
				ImGui::BeginMainMenuBar();
				{
					menuBarSize = ImGui::GetWindowSize();

					if (ImGui::BeginMenu("File"))
					{
						// TODO...
						ImGui::TextDisabled("Load Scene");
						ImGui::TextDisabled("Reload Scene");
						ImGui::TextDisabled("Reload Shaders");
						ImGui::TextDisabled("Reload Materials");

						if (ImGui::MenuItem("Quit"))
						{
							SendEvent(en::EventManager::EngineQuit);
						}

						ImGui::EndMenu();
					}

					if (ImGui::BeginMenu("Config"))
					{
						// TODO...
						ImGui::TextDisabled("Adjust input settings");

						ImGui::EndMenu();
					}

					if (ImGui::BeginMenu("Window"))
					{
						ImGui::MenuItem("Console log", "", &s_showConsoleLog); // Console debug log window

						ImGui::TextDisabled("Performance statistics");
						
						if (ImGui::BeginMenu("Entity manager"))
						{
							ImGui::MenuItem("Debug scene objects", "", &s_showEntityMgrDebug);
							ImGui::MenuItem("Debug transform hierarchy", "", &s_showTransformHierarchyDebug);
							ImGui::MenuItem("Entity/component viewer", "", &s_showEntityComponentDebug);
							ImGui::EndMenu();
						}

						if (ImGui::BeginMenu("Render manager"))
						{
							ImGui::MenuItem("Render manager debug", "", &s_showRenderMgrDebug);
							ImGui::MenuItem("Render data viewer", "", &s_showRenderDataDebug);
							ImGui::EndMenu();
						}
						

#if defined(SHOW_IMGUI_DEMO_WINDOW)
						ImGui::Separator();
						ImGui::MenuItem("Show ImGui demo", "", &s_showImguiDemo);
#endif

						ImGui::EndMenu();
					}

					if (ImGui::BeginMenu("Capture"))
					{
						// TODO...
						ImGui::TextDisabled("Save screenshot");

						ImGui::EndMenu();
					}
				}
				ImGui::EndMainMenuBar();
			};
		if (m_imguiMenuVisible)
		{
			re::RenderManager::Get()->EnqueueImGuiCommand<re::ImGuiRenderCommand<decltype(ShowMenuBar)>>(
				re::ImGuiRenderCommand<decltype(ShowMenuBar)>(ShowMenuBar));
		}

		// Console log window:
		auto ShowConsoleLog = [&]()
			{
				ImGui::SetNextWindowSize(ImVec2(
					static_cast<float>(windowWidth),
					static_cast<float>(windowHeight * 0.5f)),
					ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowPos(ImVec2(0, menuBarSize[1]), ImGuiCond_FirstUseEver, ImVec2(0, 0));

				en::LogManager::Get()->ShowImGuiWindow(&s_showConsoleLog);
			};
		if (s_showConsoleLog)
		{
			re::RenderManager::Get()->EnqueueImGuiCommand<re::ImGuiRenderCommand<decltype(ShowConsoleLog)>>(
				re::ImGuiRenderCommand<decltype(ShowConsoleLog)>(ShowConsoleLog));
		}

		// Entity manager debug:
		auto ShowEntityMgrDebug = [&]()
			{
				ImGui::SetNextWindowSize(ImVec2(
					static_cast<float>(windowWidth) * 0.25f,
					static_cast<float>(windowHeight - menuBarSize[1])),
					ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowPos(ImVec2(0, menuBarSize[1]), ImGuiCond_FirstUseEver, ImVec2(0, 0));

				fr::EntityManager::Get()->ShowImGuiWindow(
					&s_showEntityMgrDebug, &s_showTransformHierarchyDebug, &s_showEntityComponentDebug);
			};
		if (s_showEntityMgrDebug || s_showTransformHierarchyDebug || s_showEntityComponentDebug)
		{
			re::RenderManager::Get()->EnqueueImGuiCommand<re::ImGuiRenderCommand<decltype(ShowEntityMgrDebug)>>(
				re::ImGuiRenderCommand<decltype(ShowEntityMgrDebug)>(ShowEntityMgrDebug));
		}

		// Render manager debug:
		auto ShowRenderMgrDebug = [&]()
			{
				ImGui::SetNextWindowSize(ImVec2(
					static_cast<float>(windowWidth) * 0.25f,
					static_cast<float>(windowHeight - menuBarSize[1])),
					ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowPos(ImVec2(0, menuBarSize[1]), ImGuiCond_FirstUseEver, ImVec2(0, 0));

				re::RenderManager::Get()->ShowRenderManagerImGuiWindow(&s_showRenderMgrDebug);
				re::RenderManager::Get()->ShowRenderDataImGuiWindow(&s_showRenderDataDebug);
			};
		if (s_showRenderMgrDebug || s_showRenderDataDebug)
		{
			re::RenderManager::Get()->EnqueueImGuiCommand<re::ImGuiRenderCommand<decltype(ShowRenderMgrDebug)>>(
				re::ImGuiRenderCommand<decltype(ShowRenderMgrDebug)>(ShowRenderMgrDebug));
		}


		// Show the ImGui demo window for debugging reference
#if defined(SHOW_IMGUI_DEMO_WINDOW)
		auto ShowImGuiDemo = [&]()
			{
				ImGui::SetNextWindowPos(
					ImVec2(static_cast<float>(windowWidth) * 0.25f, menuBarSize[1]), ImGuiCond_FirstUseEver, ImVec2(0, 0));
				ImGui::ShowDemoWindow(&s_showImguiDemo);
			};
		if (s_showImguiDemo)
		{
			re::RenderManager::Get()->EnqueueImGuiCommand<re::ImGuiRenderCommand<decltype(ShowImGuiDemo)>>(
				re::ImGuiRenderCommand<decltype(ShowImGuiDemo)>(ShowImGuiDemo));
		}
#endif
	}
}