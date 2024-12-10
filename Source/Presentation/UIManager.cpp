// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "SceneManager.h"
#include "UIManager.h"

#include "Core/CommandQueue.h"
#include "Core/Config.h"
#include "Core/InputManager_Platform.h"
#include "Core/LogManager.h"

#include "Core/Definitions/KeyConfiguration.h"

#include "Renderer/Context.h"
#include "Renderer/GraphicsSystem_ImGui.h"
#include "Renderer/RenderManager.h"


namespace
{
	// Helper wrapper to reduce boilerplate
	void SendEvent(core::EventManager::EventType eventType)
	{
		core::EventManager::Get()->Notify(core::EventManager::EventInfo{
				.m_type = eventType,
				//.m_data0 = ,
				//.m_data1 = 
			});
	}


	void AddKeyEventToImGui(ImGuiIO& io, definitions::SEKeycode keycode, bool keystate)
	{
		// Keyboard sections: left to right, row-by-row
		switch (keycode)
		{
		case definitions::SEK_ESCAPE: io.AddKeyEvent(ImGuiKey_Escape, keystate); break;
		case definitions::SEK_F1: io.AddKeyEvent(ImGuiKey_F1, keystate); break;
		case definitions::SEK_F2: io.AddKeyEvent(ImGuiKey_F2, keystate); break;
		case definitions::SEK_F3: io.AddKeyEvent(ImGuiKey_F3, keystate); break;
		case definitions::SEK_F4: io.AddKeyEvent(ImGuiKey_F4, keystate); break;
		case definitions::SEK_F5: io.AddKeyEvent(ImGuiKey_F5, keystate); break;
		case definitions::SEK_F6: io.AddKeyEvent(ImGuiKey_F6, keystate); break;
		case definitions::SEK_F7: io.AddKeyEvent(ImGuiKey_F7, keystate); break;
		case definitions::SEK_F8: io.AddKeyEvent(ImGuiKey_F8, keystate); break;
		case definitions::SEK_F9: io.AddKeyEvent(ImGuiKey_F9, keystate); break;
		case definitions::SEK_F10: io.AddKeyEvent(ImGuiKey_F10, keystate); break;
		case definitions::SEK_F11: io.AddKeyEvent(ImGuiKey_F11, keystate); break;
		case definitions::SEK_F12: io.AddKeyEvent(ImGuiKey_F12, keystate); break;

		case definitions::SEK_GRAVE: io.AddKeyEvent(ImGuiKey_GraveAccent, keystate); break;
		case definitions::SEK_0: io.AddKeyEvent(ImGuiKey_0, keystate); break;
		case definitions::SEK_1: io.AddKeyEvent(ImGuiKey_1, keystate); break;
		case definitions::SEK_2: io.AddKeyEvent(ImGuiKey_2, keystate); break;
		case definitions::SEK_3: io.AddKeyEvent(ImGuiKey_3, keystate); break;
		case definitions::SEK_4: io.AddKeyEvent(ImGuiKey_4, keystate); break;
		case definitions::SEK_5: io.AddKeyEvent(ImGuiKey_5, keystate); break;
		case definitions::SEK_6: io.AddKeyEvent(ImGuiKey_6, keystate); break;
		case definitions::SEK_7: io.AddKeyEvent(ImGuiKey_7, keystate); break;
		case definitions::SEK_8: io.AddKeyEvent(ImGuiKey_8, keystate); break;
		case definitions::SEK_9: io.AddKeyEvent(ImGuiKey_9, keystate); break;
		case definitions::SEK_MINUS: io.AddKeyEvent(ImGuiKey_Minus, keystate); break;
		case definitions::SEK_EQUALS: io.AddKeyEvent(ImGuiKey_Equal, keystate); break;
		case definitions::SEK_BACKSPACE: io.AddKeyEvent(ImGuiKey_Backspace, keystate); break;

		case definitions::SEK_TAB: io.AddKeyEvent(ImGuiKey_Tab, keystate); break;
		case definitions::SEK_Q: io.AddKeyEvent(ImGuiKey_Q, keystate); break;
		case definitions::SEK_W: io.AddKeyEvent(ImGuiKey_W, keystate); break;
		case definitions::SEK_E: io.AddKeyEvent(ImGuiKey_E, keystate); break;
		case definitions::SEK_R: io.AddKeyEvent(ImGuiKey_R, keystate); break;
		case definitions::SEK_T: io.AddKeyEvent(ImGuiKey_T, keystate); break;
		case definitions::SEK_Y: io.AddKeyEvent(ImGuiKey_Y, keystate); break;
		case definitions::SEK_U: io.AddKeyEvent(ImGuiKey_U, keystate); break;
		case definitions::SEK_I: io.AddKeyEvent(ImGuiKey_I, keystate); break;
		case definitions::SEK_O: io.AddKeyEvent(ImGuiKey_O, keystate); break;
		case definitions::SEK_P: io.AddKeyEvent(ImGuiKey_P, keystate); break;
		case definitions::SEK_LEFTBRACKET: io.AddKeyEvent(ImGuiKey_LeftBracket, keystate); break;
		case definitions::SEK_RIGHTBRACKET: io.AddKeyEvent(ImGuiKey_RightBracket, keystate); break;
		case definitions::SEK_BACKSLASH: io.AddKeyEvent(ImGuiKey_Backslash, keystate); break;

		case definitions::SEK_CAPSLOCK: io.AddKeyEvent(ImGuiKey_CapsLock, keystate); break;
		case definitions::SEK_A: io.AddKeyEvent(ImGuiKey_A, keystate); break;
		case definitions::SEK_S: io.AddKeyEvent(ImGuiKey_S, keystate); break;
		case definitions::SEK_D: io.AddKeyEvent(ImGuiKey_D, keystate); break;
		case definitions::SEK_F: io.AddKeyEvent(ImGuiKey_F, keystate); break;
		case definitions::SEK_G: io.AddKeyEvent(ImGuiKey_G, keystate); break;
		case definitions::SEK_H: io.AddKeyEvent(ImGuiKey_H, keystate); break;
		case definitions::SEK_J: io.AddKeyEvent(ImGuiKey_J, keystate); break;
		case definitions::SEK_K: io.AddKeyEvent(ImGuiKey_K, keystate); break;
		case definitions::SEK_L: io.AddKeyEvent(ImGuiKey_L, keystate); break;
		case definitions::SEK_SEMICOLON: io.AddKeyEvent(ImGuiKey_Semicolon, keystate); break;
		case definitions::SEK_APOSTROPHE: io.AddKeyEvent(ImGuiKey_Apostrophe, keystate); break;
		case definitions::SEK_RETURN: io.AddKeyEvent(ImGuiKey_Enter, keystate); break;

		case definitions::SEK_LSHIFT: io.AddKeyEvent(ImGuiKey_LeftShift, keystate); break;
		case definitions::SEK_Z: io.AddKeyEvent(ImGuiKey_Z, keystate); break;
		case definitions::SEK_X: io.AddKeyEvent(ImGuiKey_X, keystate); break;
		case definitions::SEK_C: io.AddKeyEvent(ImGuiKey_C, keystate); break;
		case definitions::SEK_V: io.AddKeyEvent(ImGuiKey_V, keystate); break;
		case definitions::SEK_B: io.AddKeyEvent(ImGuiKey_B, keystate); break;
		case definitions::SEK_N: io.AddKeyEvent(ImGuiKey_N, keystate); break;
		case definitions::SEK_M: io.AddKeyEvent(ImGuiKey_M, keystate); break;
		case definitions::SEK_COMMA: io.AddKeyEvent(ImGuiKey_Comma, keystate); break;
		case definitions::SEK_PERIOD: io.AddKeyEvent(ImGuiKey_Period, keystate); break;
		case definitions::SEK_SLASH: io.AddKeyEvent(ImGuiKey_Slash, keystate); break;
		case definitions::SEK_RSHIFT: io.AddKeyEvent(ImGuiKey_RightShift, keystate); break;

		case definitions::SEK_LCTRL: io.AddKeyEvent(ImGuiKey_LeftCtrl, keystate); break;
		case definitions::SEK_APPLICATION: io.AddKeyEvent(ImGuiKey_Menu, keystate); break; // ?
		case definitions::SEK_LALT: io.AddKeyEvent(ImGuiKey_LeftAlt, keystate); break;
		case definitions::SEK_SPACE: io.AddKeyEvent(ImGuiKey_Space, keystate); break;
		case definitions::SEK_RALT: io.AddKeyEvent(ImGuiKey_RightAlt, keystate); break;
		case definitions::SEK_RCTRL: io.AddKeyEvent(ImGuiKey_RightCtrl, keystate); break;

		case definitions::SEK_PRINTSCREEN: io.AddKeyEvent(ImGuiKey_PrintScreen, keystate); break;
		case definitions::SEK_SCROLLLOCK: io.AddKeyEvent(ImGuiKey_ScrollLock, keystate); break;
		case definitions::SEK_PAUSE: io.AddKeyEvent(ImGuiKey_Pause, keystate); break;

		case definitions::SEK_INSERT: io.AddKeyEvent(ImGuiKey_Insert, keystate); break;
		case definitions::SEK_HOME: io.AddKeyEvent(ImGuiKey_Home, keystate); break;
		case definitions::SEK_PAGEUP: io.AddKeyEvent(ImGuiKey_PageUp, keystate); break;

		case definitions::SEK_DELETE: io.AddKeyEvent(ImGuiKey_Delete, keystate); break;
		case definitions::SEK_END: io.AddKeyEvent(ImGuiKey_End, keystate); break;
		case definitions::SEK_PAGEDOWN: io.AddKeyEvent(ImGuiKey_PageDown, keystate); break;

		case definitions::SEK_UP: io.AddKeyEvent(ImGuiKey_UpArrow, keystate); break;
		case definitions::SEK_DOWN: io.AddKeyEvent(ImGuiKey_DownArrow, keystate); break;
		case definitions::SEK_LEFT: io.AddKeyEvent(ImGuiKey_LeftArrow, keystate); break;
		case definitions::SEK_RIGHT: io.AddKeyEvent(ImGuiKey_RightArrow, keystate);	break;

		case definitions::SEK_NUMLOCK: io.AddKeyEvent(ImGuiKey_NumLock, keystate);	break;

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
		: m_debugUIRenderSystemCreated(false)
		, m_debugUICommandMgr(nullptr)
		, m_imguiGlobalMutex(nullptr)
		, m_imguiMenuVisible(false)
		, m_prevImguiMenuVisible(false)
		, m_imguiWantsToCaptureKeyboard(false)
		, m_imguiWantsToCaptureMouse(false)
	{
	}


	void UIManager::Startup()
	{
		LOG("UI manager starting...");

		// Event subscriptions:
		core::EventManager::Get()->Subscribe(core::EventManager::EventType::InputToggleConsole, this);

		// Input events:
		core::EventManager::Get()->Subscribe(core::EventManager::EventType::TextInputEvent, this);
		core::EventManager::Get()->Subscribe(core::EventManager::EventType::KeyEvent, this);
		core::EventManager::Get()->Subscribe(core::EventManager::EventType::MouseMotionEvent, this);
		core::EventManager::Get()->Subscribe(core::EventManager::EventType::MouseButtonEvent, this);
		core::EventManager::Get()->Subscribe(core::EventManager::EventType::MouseWheelEvent, this);

		// Create UI render systems:
		std::atomic<bool>* createdFlag = &m_debugUIRenderSystemCreated;
		core::FrameIndexedCommandManager** cmdMgrPtr = &m_debugUICommandMgr;
		std::mutex** imguiMutexPtr = &m_imguiGlobalMutex;
		std::function<void()> test = [createdFlag, cmdMgrPtr, imguiMutexPtr]()
			{
				constexpr char const* k_debugUIRenderSystemName = "DebugImGui";
				constexpr char const* k_debugUIPipelineFilename = "ui.json";

				re::RenderSystem const* debugUIRenderSystem = re::RenderManager::Get()->CreateAddRenderSystem(
					k_debugUIRenderSystemName,
					k_debugUIPipelineFilename);

				gr::GraphicsSystemManager const& gsm = debugUIRenderSystem->GetGraphicsSystemManager();

				gr::ImGuiGraphicsSystem* debugUIGraphicsSystem = gsm.GetGraphicsSystem<gr::ImGuiGraphicsSystem>();

				*cmdMgrPtr = debugUIGraphicsSystem->GetFrameIndexedCommandManager();
				*imguiMutexPtr = &debugUIGraphicsSystem->GetGlobalImGuiMutex();

				createdFlag->store(true);
			};

		re::RenderManager::Get()->EnqueueRenderCommand(std::move(test));
	}


	void UIManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		SEAssert(!m_debugUIRenderSystemCreated || (m_debugUICommandMgr && m_imguiGlobalMutex),
			"One of our GS pointers is null");

		HandleEvents();

		if (m_debugUIRenderSystemCreated)
		{
			// ImGui visibility state has changed:
			if (m_imguiMenuVisible != m_prevImguiMenuVisible)
			{
				m_prevImguiMenuVisible = m_imguiMenuVisible;

				// If true, hide the mouse and lock it to the window
				const bool captureMouse = !m_imguiMenuVisible;
				re::Context::Get()->GetWindow()->SetRelativeMouseMode(captureMouse);

				// Disable ImGui mouse listening if the console is not active: Prevents UI elements
				// flashing as the hidden mouse cursor passes by

				{
					std::lock_guard<std::mutex> lock(*m_imguiGlobalMutex);

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
						std::lock_guard<std::mutex> lock(*m_imguiGlobalMutex);
						ImGuiIO& io = ImGui::GetIO();
						currentImguiWantsToCaptureKeyboard = io.WantCaptureKeyboard;
						currentImguiWantsToCaptureMouse = io.WantCaptureMouse;
					}

					if (currentImguiWantsToCaptureKeyboard != m_imguiWantsToCaptureKeyboard)
					{
						m_imguiWantsToCaptureKeyboard = currentImguiWantsToCaptureKeyboard;

						core::EventManager::Get()->Notify(core::EventManager::EventInfo{
							.m_type = core::EventManager::EventType::KeyboardInputCaptureChange,
							.m_data0 = core::EventManager::EventData{.m_dataB = m_imguiWantsToCaptureKeyboard },
							//.m_data1 = 
							});
					}

					if (currentImguiWantsToCaptureMouse != m_imguiWantsToCaptureMouse)
					{
						m_imguiWantsToCaptureMouse = currentImguiWantsToCaptureMouse;

						core::EventManager::Get()->Notify(core::EventManager::EventInfo{
							.m_type = core::EventManager::EventType::MouseInputCaptureChange,
							.m_data0 = core::EventManager::EventData{.m_dataB = m_imguiWantsToCaptureMouse },
							//.m_data1 = 
							});
					}
				}
			}

			SubmitImGuiRenderCommands(frameNum);
		}
	}


	void UIManager::Shutdown()
	{
		LOG("UI manager shutting down...");
		m_imguiMenuVisible = false;
	}


	void UIManager::HandleEvents()
	{
		// Cache this once to prevent a race where it changes midway through processing
		const bool debugUISystemCreated = m_debugUIRenderSystemCreated;

		while (HasEvents())
		{
			core::EventManager::EventInfo const& eventInfo = GetEvent();

			switch (eventInfo.m_type)
			{
			case core::EventManager::EventType::InputToggleConsole:
			{
				if (eventInfo.m_data0.m_dataB)
				{
					m_imguiMenuVisible = !m_imguiMenuVisible;
				}
			}
			break;
			case core::EventManager::EventType::TextInputEvent:
			{
				if (debugUISystemCreated)
				{
					std::lock_guard<std::mutex> lock(*m_imguiGlobalMutex);
					ImGuiIO& io = ImGui::GetIO();
					io.AddInputCharacter(eventInfo.m_data0.m_dataC);
				}
			}
			break;
			case core::EventManager::KeyEvent:
			{
				const definitions::SEKeycode keycode = platform::InputManager::ConvertToSEKeycode(eventInfo.m_data0.m_dataUI);
				const bool keystate = eventInfo.m_data1.m_dataB;

				// We always broadcast to ImGui, even if it doesn't want exclusive capture of input
				if (debugUISystemCreated)
				{
					std::lock_guard<std::mutex> lock(*m_imguiGlobalMutex);
					ImGuiIO& io = ImGui::GetIO();
					AddKeyEventToImGui(io, keycode, keystate);
				}
			}
			break;
			case core::EventManager::MouseButtonEvent:
			{
				const bool buttonState = eventInfo.m_data1.m_dataB;
				
				if (debugUISystemCreated)
				{
					std::lock_guard<std::mutex> lock(*m_imguiGlobalMutex);
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
			case core::EventManager::MouseWheelEvent:
			{
				if (debugUISystemCreated)
				{
					std::lock_guard<std::mutex> lock(*m_imguiGlobalMutex);
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


	void UIManager::SubmitImGuiRenderCommands(uint64_t frameNum)
	{
		// Importantly, this function does NOT modify any ImGui state. Instead, it submits commands to the render
		// manager, which will execute the updates on the render thread

		enum Show : uint8_t
		{
			LogConsole,
			SceneMgrDbg,
			EntityMgrDbg,
			TransformationHierarchyDbg,
			EntityComponentDbg,
			RenderMgrDbg,
			RenderDataDbg,
			LightMgrDbg,
			GPUCaptures,
			
			ImGuiDemo,

			Show_Count
		};
		static std::array<bool, Show::Show_Count> s_show = {0};



//#define FORCE_SHOW_IMGUI_DEMO
#if defined(_DEBUG) || defined(FORCE_SHOW_IMGUI_DEMO)
#define SHOW_IMGUI_DEMO_WINDOW
#endif

		// Early out if we can
		bool showAny = false;
		if (!m_imguiMenuVisible)
		{
			for (bool show : s_show)
			{
				if (show)
				{
					showAny = true;
					break;
				}
			}
			if (!showAny)
			{
				return;
			}
		}
		
		static const int windowWidth = core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
		static const int windowHeight = core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);

		static ImVec2 menuBarSize = { 0, 0 }; // Record the size of the menu bar so we can align things absolutely underneath it


		// Create a hidden dock node to keep our dock space active
		// Note: Our showAny logic above means this is only ever submitted if there is a window actually visible
		m_debugUICommandMgr->Enqueue(frameNum,
			[]()
			{
				const ImGuiWindowFlags passthroughDockingWindowFlags =
					ImGuiWindowFlags_NoDocking |
					ImGuiWindowFlags_NoTitleBar |
					ImGuiWindowFlags_NoCollapse |
					ImGuiWindowFlags_NoResize |
					ImGuiWindowFlags_NoMove |
					ImGuiWindowFlags_NoBringToFrontOnFocus |
					ImGuiWindowFlags_NoNavFocus |
					ImGuiWindowFlags_NoBackground;

				const ImGuiViewport* viewport = ImGui::GetMainViewport();

				ImGui::SetNextWindowPos(viewport->WorkPos);
				ImGui::SetNextWindowSize(viewport->WorkSize);
				ImGui::SetNextWindowViewport(viewport->ID);

				ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

				ImGui::Begin("SaberEngineMainDockSpace", nullptr, passthroughDockingWindowFlags);

				ImGui::PopStyleVar(3);

				// Submit the DockSpace:
				ImGuiID dockspaceID = ImGui::GetID("SaberEngineMainDockSpaceID");
				ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

				ImGui::End();
			});


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
							SendEvent(core::EventManager::EngineQuit);
						}

						ImGui::EndMenu();
					}

					if (ImGui::BeginMenu("Config"))
					{
						const bool currentVSyncEnabled = re::RenderManager::Get()->IsVSyncEnabled();
						bool vsyncEnabled = currentVSyncEnabled;
						ImGui::Checkbox("V-Sync", &vsyncEnabled);
						if (currentVSyncEnabled != vsyncEnabled)
						{
							re::RenderManager::Get()->ToggleVSync();
						}
						ImGui::EndMenu();
					}

					if (ImGui::BeginMenu("Window"))
					{
						ImGui::MenuItem("Console log", "", &s_show[Show::LogConsole]); // Console debug log window
						
						if (ImGui::BeginMenu("Scene manager"))
						{
							ImGui::MenuItem("Spawn scene objects", "", &s_show[Show::SceneMgrDbg]);
							ImGui::EndMenu();
						}

						if (ImGui::BeginMenu("Entity manager"))
						{
							ImGui::MenuItem("Scene objects", "", &s_show[Show::EntityMgrDbg]);
							ImGui::MenuItem("Node hierarchy", "", &s_show[Show::EntityComponentDbg]);
							ImGui::MenuItem("Transform hierarchy", "", &s_show[Show::TransformationHierarchyDbg]);
							ImGui::EndMenu();
						}

						if (ImGui::BeginMenu("Render manager"))
						{
							ImGui::MenuItem("Render Systems", "", &s_show[Show::RenderMgrDbg]);
							ImGui::MenuItem("Render data debug", "", &s_show[Show::RenderDataDbg]);
							ImGui::MenuItem("Light manager debug", "", &s_show[Show::LightMgrDbg]);
							ImGui::EndMenu();
						}
						

#if defined(SHOW_IMGUI_DEMO_WINDOW)
						ImGui::Separator();
						ImGui::MenuItem("Show ImGui demo", "", &s_show[Show::ImGuiDemo]);
#endif

						ImGui::EndMenu();
					}

					if (ImGui::BeginMenu("Capture"))
					{
						ImGui::TextDisabled("Performance statistics");

						ImGui::MenuItem("GPU Captures", "", &s_show[Show::GPUCaptures]);

						// TODO...
						ImGui::TextDisabled("Save screenshot");

						ImGui::EndMenu();
					}
				}
				ImGui::EndMainMenuBar();
			};
		if (m_imguiMenuVisible)
		{
			m_debugUICommandMgr->Enqueue(frameNum, ShowMenuBar);
		}

		// Console log window:
		auto ShowConsoleLog = [&]()
			{
				ImGui::SetNextWindowSize(ImVec2(
					static_cast<float>(windowWidth),
					static_cast<float>(windowHeight * 0.5f)),
					ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowPos(ImVec2(0, menuBarSize[1]), ImGuiCond_FirstUseEver, ImVec2(0, 0));

				core::LogManager::Get()->ShowImGuiWindow(&s_show[Show::LogConsole]);
			};
		if (s_show[Show::LogConsole])
		{
			m_debugUICommandMgr->Enqueue(frameNum, ShowConsoleLog);
		}

		// Scene manager debug:
		auto ShowSceneMgrDebug = [&]()
			{
				fr::SceneManager::Get()->ShowImGuiWindow(&s_show[Show::SceneMgrDbg]);
			};
		if (s_show[Show::SceneMgrDbg])
		{
			m_debugUICommandMgr->Enqueue(frameNum, ShowSceneMgrDebug);
		}

		// Entity manager debug:
		auto ShowEntityMgrDebug = [&]()
			{
				fr::EntityManager::Get()->ShowSceneObjectsImGuiWindow(&s_show[Show::EntityMgrDbg]);
				fr::EntityManager::Get()->ShowSceneTransformImGuiWindow(&s_show[Show::TransformationHierarchyDbg]);
				fr::EntityManager::Get()->ShowImGuiEntityComponentDebug(&s_show[Show::EntityComponentDbg]);
			};
		if (s_show[Show::EntityMgrDbg] || s_show[Show::TransformationHierarchyDbg] || s_show[Show::EntityComponentDbg])
		{
			m_debugUICommandMgr->Enqueue(frameNum, ShowEntityMgrDebug);
		}

		// Render manager debug:
		auto ShowRenderMgrDebug = [&]()
			{
				ImGui::SetNextWindowSize(ImVec2(
					static_cast<float>(windowWidth) * 0.25f,
					static_cast<float>(windowHeight - menuBarSize[1])),
					ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowPos(ImVec2(0, menuBarSize[1]), ImGuiCond_FirstUseEver, ImVec2(0, 0));

				re::RenderManager::Get()->ShowRenderSystemsImGuiWindow(&s_show[Show::RenderMgrDbg]);
				re::RenderManager::Get()->ShowRenderDataImGuiWindow(&s_show[Show::RenderDataDbg]);
				re::RenderManager::Get()->ShowLightManagerImGuiWindow(&s_show[Show::LightMgrDbg]);
				re::RenderManager::Get()->ShowGPUCapturesImGuiWindow(&s_show[Show::GPUCaptures]);
				
			};
		if (s_show[Show::RenderMgrDbg] ||
			s_show[Show::RenderDataDbg] ||
			s_show[Show::LightMgrDbg] ||
			s_show[Show::GPUCaptures])
		{
			m_debugUICommandMgr->Enqueue(frameNum, ShowRenderMgrDebug);
		}


		// Show the ImGui demo window for debugging reference
#if defined(SHOW_IMGUI_DEMO_WINDOW)
		auto ShowImGuiDemo = [&]()
			{
				ImGui::SetNextWindowPos(
					ImVec2(static_cast<float>(windowWidth) * 0.25f, menuBarSize[1]), ImGuiCond_FirstUseEver, ImVec2(0, 0));
				ImGui::ShowDemoWindow(&s_show[Show::ImGuiDemo]);
			};
		if (s_show[Show::ImGuiDemo])
		{
			m_debugUICommandMgr->Enqueue(frameNum, ShowImGuiDemo);
		}
#endif
	}
}