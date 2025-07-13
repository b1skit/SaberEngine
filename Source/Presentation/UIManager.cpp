// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "SceneManager.h"
#include "UIManager.h"

#include "Core/CommandQueue.h"
#include "Core/Config.h"
#include "Core/InputManager_Platform.h"
#include "Core/Logger.h"
#include "Core/PerfLogger.h"

#include "Core/Definitions/EventKeys.h"
#include "Core/Definitions/KeyConfiguration.h"

#include "Core/Host/Dialog.h"
#include "Core/Host/Window.h"

#include "Renderer/GraphicsSystem_ImGui.h"
#include "Renderer/RenderManager.h"


namespace
{
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


	void FileImport()
	{
		core::ThreadPool::Get()->EnqueueJob([]()
			{
				std::string requestedFilepath;
				const bool didRequestFile = host::Dialog::OpenFileDialogBox(
					"Scene Files",
					{"*.gltf", "*.glb", "*.hdr"},
					requestedFilepath);

				if (didRequestFile)
				{
					core::EventManager::Get()->Notify(core::EventManager::EventInfo{
						.m_eventKey = eventkey::FileImportRequest,
						.m_data = requestedFilepath });
				}
			});
	}
}

namespace pr
{
	UIManager* UIManager::Get()
	{
		static std::unique_ptr<pr::UIManager> instance = std::make_unique<pr::UIManager>();
		return instance.get();
	}


	UIManager::UIManager()
		: m_debugUIRenderSystemCreated(false)
		, m_debugUICommandMgr(nullptr)
		, m_imguiGlobalMutex(nullptr)
		, m_showImGui(true)
		, m_imguiMenuActive(true)
		, m_prevImguiMenuActive(false)
		, m_imguiWantsToCaptureKeyboard(false)
		, m_imguiWantsToCaptureMouse(false)
		, m_imguiWantsTextInput(false)
		, m_show{0}
		, m_window(nullptr)
		, m_vsyncState(false) // Will be updated by the initial state broadcast event
	{
	}


	void UIManager::Startup()
	{
		SEAssert(m_window, "Window should have been set by now");

		LOG("UI manager starting...");

		// Event subscriptions:
		// Input events:
		core::EventManager::Get()->Subscribe(eventkey::TextInputEvent, this);
		core::EventManager::Get()->Subscribe(eventkey::KeyEvent, this);
		core::EventManager::Get()->Subscribe(eventkey::MouseMotionEvent, this);
		core::EventManager::Get()->Subscribe(eventkey::MouseButtonEvent, this);
		core::EventManager::Get()->Subscribe(eventkey::MouseWheelEvent, this);
		core::EventManager::Get()->Subscribe(eventkey::DragAndDrop, this);
		core::EventManager::Get()->Subscribe(eventkey::VSyncModeChanged, this);
		core::EventManager::Get()->Subscribe(eventkey::ToggleConsole, this);
		core::EventManager::Get()->Subscribe(eventkey::ToggleUIVisibility, this);

		// Create UI render systems:
		std::atomic<bool>* createdFlag = &m_debugUIRenderSystemCreated;
		core::FrameIndexedCommandManager** cmdMgrPtr = &m_debugUICommandMgr;
		std::mutex** imguiMutexPtr = &m_imguiGlobalMutex;

		gr::RenderManager::Get()->EnqueueRenderCommand([createdFlag, cmdMgrPtr, imguiMutexPtr]()
			{
				constexpr char const* k_debugUIPipelineFilename = "UI.json";

				gr::RenderSystem const* debugUIRenderSystem = 
					gr::RenderManager::Get()->CreateAddRenderSystem(k_debugUIPipelineFilename);

				gr::GraphicsSystemManager const& gsm = debugUIRenderSystem->GetGraphicsSystemManager();

				gr::ImGuiGraphicsSystem* debugUIGraphicsSystem = gsm.GetGraphicsSystem<gr::ImGuiGraphicsSystem>();

				*cmdMgrPtr = debugUIGraphicsSystem->GetFrameIndexedCommandManager();
				*imguiMutexPtr = &debugUIGraphicsSystem->GetGlobalImGuiMutex();

				createdFlag->store(true);
			});

		// Default visible debug ImGui panels:
		m_show[Logger] = true;
		m_show[PerfLogger] = true;
		m_show[SceneMgrDbg] = true;
		m_show[EntityMgrDbg] = true;
		m_show[TransformationHierarchyDbg] = true;
		m_show[EntityComponentDbg] = true;
		m_show[RenderMgrDbg] = true;

		core::EventManager::Get()->Notify(core::EventManager::EventInfo{
			.m_eventKey = eventkey::TogglePerformanceTimers,
			.m_data = m_show[PerfLogger],
			});

		m_window->SetRelativeMouseMode(!m_imguiMenuActive);

		// Service initialization:
		m_cullingGraphicsService.Initialize(gr::RenderManager::Get());
		m_debugGraphicsService.Initialize(gr::RenderManager::Get());
	}


	void UIManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		SEAssert(!m_debugUIRenderSystemCreated.load() || (m_debugUICommandMgr && m_imguiGlobalMutex),
			"One of our GS pointers is null");

		HandleEvents();

		if (!m_showImGui)
		{
			return;
		}

		if (m_debugUIRenderSystemCreated.load())
		{
			// Update ImGui visibility state:
			const bool imguiVisiblityChanged = m_imguiMenuActive != m_prevImguiMenuActive;
			m_prevImguiMenuActive = m_imguiMenuActive;
			
			// Update ImGui input capture states:
			{
				std::lock_guard<std::mutex> lock(*m_imguiGlobalMutex);

				ImGuiIO& io = ImGui::GetIO();

				m_imguiWantsToCaptureKeyboard = io.WantCaptureKeyboard;
				m_imguiWantsToCaptureMouse = io.WantCaptureMouse;
				m_imguiWantsTextInput = io.WantTextInput;

				// Disable ImGui mouse listening if the console is not active: Prevents UI elements
				// flashing as the hidden mouse cursor passes by
				if (m_imguiMenuActive)
				{
					io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
				}
				else
				{
					io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
				}
			}

			// Capture the input if the ImGui menu bar is visible, or if ImGui explicitely requests it:
			const bool imguiWantsButtonCapture = m_imguiWantsToCaptureKeyboard || m_imguiWantsTextInput;
			if (imguiVisiblityChanged || imguiWantsButtonCapture)
			{
				core::EventManager::Get()->Notify(core::EventManager::EventInfo{
					.m_eventKey = eventkey::KeyboardInputCaptureChange,
					.m_data = (m_imguiMenuActive || imguiWantsButtonCapture), });
			}

			if (imguiVisiblityChanged || m_imguiWantsToCaptureMouse)
			{
				core::EventManager::Get()->Notify(core::EventManager::EventInfo{
					.m_eventKey = eventkey::MouseInputCaptureChange,
					.m_data = (m_imguiMenuActive || m_imguiWantsToCaptureMouse), });
			}

			SubmitImGuiRenderCommands(frameNum);
		}
	}


	void UIManager::Shutdown()
	{
		LOG("UI manager shutting down...");
		m_imguiMenuActive = false;
	}


	void UIManager::HandleEvents()
	{
		// Cache this once to prevent a race where it changes midway through processing
		const bool debugUISystemCreated = m_debugUIRenderSystemCreated.load();

		while (HasEvents())
		{
			core::EventManager::EventInfo const& eventInfo = GetEvent();

			switch (eventInfo.m_eventKey)
			{
			case eventkey::ToggleConsole:
			{
				// Only respond to console toggle events if we're not typing
				if (!m_imguiWantsToCaptureKeyboard && !m_imguiWantsTextInput)
				{
					m_imguiMenuActive = !m_imguiMenuActive;

					// If ImGui is not visible, hide the mouse and lock it to the window
					m_window->SetRelativeMouseMode(!m_imguiMenuActive);
				}
			}
			break;
			case eventkey::ToggleUIVisibility:
			{
				m_showImGui = !m_showImGui;

				// Enable/disable the performance logging, for efficiency
				if (m_show[PerfLogger])
				{
					core::EventManager::Get()->Notify(core::EventManager::EventInfo{
						.m_eventKey = eventkey::TogglePerformanceTimers,
						.m_data = m_showImGui, });
				}
			}
			break;
			case eventkey::TextInputEvent:
			{
				if (debugUISystemCreated)
				{
					std::lock_guard<std::mutex> lock(*m_imguiGlobalMutex);
					ImGuiIO& io = ImGui::GetIO();
					io.AddInputCharacter(std::get<char>(eventInfo.m_data));
				}
			}
			break;
			case eventkey::KeyEvent:
			{
				std::pair<uint32_t, bool> const& data = std::get<std::pair<uint32_t, bool>>(eventInfo.m_data);

				const definitions::SEKeycode keycode = platform::InputManager::ConvertToSEKeycode(data.first);
				const bool keystate = data.second;

				// We always broadcast to ImGui, even if it doesn't want exclusive capture of input
				if (debugUISystemCreated)
				{
					std::lock_guard<std::mutex> lock(*m_imguiGlobalMutex);
					ImGuiIO& io = ImGui::GetIO();
					AddKeyEventToImGui(io, keycode, keystate);
				}
			}
			break;
			case eventkey::MouseButtonEvent:
			{
				std::pair<uint32_t, bool> const& data = std::get<std::pair<uint32_t, bool>>(eventInfo.m_data);

				const bool buttonState = data.second;
				
				if (debugUISystemCreated && m_imguiMenuActive)
				{
					std::lock_guard<std::mutex> lock(*m_imguiGlobalMutex);
					ImGuiIO& io = ImGui::GetIO();

					switch (data.first)
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
			case eventkey::MouseWheelEvent:
			{
				if (debugUISystemCreated)
				{
					std::lock_guard<std::mutex> lock(*m_imguiGlobalMutex);
					ImGuiIO& io = ImGui::GetIO();

					std::pair<int32_t, int32_t> const& data = std::get<std::pair<int32_t, int32_t>>(eventInfo.m_data);

					io.AddMouseWheelEvent(static_cast<float>(data.first), static_cast<float>(data.second));
				}
			}
			break;
			case eventkey::DragAndDrop:
			{
				std::string const& filePath = std::get<std::string>(eventInfo.m_data);

				core::EventManager::Get()->Notify(core::EventManager::EventInfo{
					.m_eventKey = eventkey::FileImportRequest, 
					.m_data = filePath });
			}
			break;
			case eventkey::VSyncModeChanged:
			{
				m_vsyncState = std::get<bool>(eventInfo.m_data);

				LOG("VSync %s", m_vsyncState ? "enabled" : "disabled");
			}
			break;
			default:
				break;
			}
		}
	}


	void UIManager::SetWindow(host::Window* window)
	{
		m_window = window;
	}


	void UIManager::SubmitImGuiRenderCommands(uint64_t frameNum)
	{
		// Importantly, this function does NOT modify any ImGui state. Instead, it submits commands to the render
		// manager, which will execute the updates on the render thread


//#define FORCE_SHOW_IMGUI_DEMO
#if defined(_DEBUG) || defined(FORCE_SHOW_IMGUI_DEMO)
#define SHOW_IMGUI_DEMO_WINDOW
#endif

		// Early out if we can
		bool showAny = false;
		if (!m_imguiMenuActive)
		{
			for (bool show : m_show)
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
		
		static const int windowWidth = core::Config::GetValue<int>(core::configkeys::k_windowWidthKey);
		static const int windowHeight = core::Config::GetValue<int>(core::configkeys::k_windowHeightKey);

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

				if (ImGui::Begin("SaberEngineMainDockSpace", nullptr, passthroughDockingWindowFlags))
				{

					ImGui::PopStyleVar(3);

					// Submit the DockSpace:
					ImGuiID dockspaceID = ImGui::GetID("SaberEngineMainDockSpaceID");
					ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
				}
				ImGui::End();
			});


		// Menu bar:
		auto ShowMenuBar = [&]()
			{	
				if (ImGui::BeginMainMenuBar())
				{
					menuBarSize = ImGui::GetWindowSize();

					if (ImGui::BeginMenu("File"))
					{
						if (ImGui::MenuItem("Import"))
						{
							FileImport();
						}

						if (ImGui::MenuItem("Reset"))
						{
							core::EventManager::Get()->Notify(core::EventManager::EventInfo{
								.m_eventKey = eventkey::SceneResetRequest, });
						}

						ImGui::Separator();

						if (ImGui::MenuItem("Quit"))
						{
							core::EventManager::Get()->Notify(core::EventManager::EventInfo{
								.m_eventKey = eventkey::EngineQuit, });
						}

						ImGui::EndMenu();
					}

					if (ImGui::BeginMenu("Config"))
					{
						if (ImGui::Checkbox("V-Sync", &m_vsyncState))
						{
							core::EventManager::Get()->Notify(core::EventManager::EventInfo{
								.m_eventKey = eventkey::ToggleVSync, });
						}
						ImGui::EndMenu();
					}

					if (ImGui::BeginMenu("Window"))
					{
						ImGui::MenuItem("Log", "", &m_show[Show::Logger]);
						
						if (ImGui::BeginMenu("Scene manager"))
						{
							ImGui::MenuItem("Spawn scene objects", "", &m_show[Show::SceneMgrDbg]);
							ImGui::EndMenu();
						}

						if (ImGui::BeginMenu("Entity manager"))
						{
							ImGui::MenuItem("Scene objects", "", &m_show[Show::EntityMgrDbg]);
							ImGui::MenuItem("Node hierarchy", "", &m_show[Show::EntityComponentDbg]);
							ImGui::MenuItem("Transform hierarchy", "", &m_show[Show::TransformationHierarchyDbg]);
							ImGui::EndMenu();
						}

						if (ImGui::BeginMenu("Render manager"))
						{
							ImGui::MenuItem("Render Systems", "", &m_show[Show::RenderMgrDbg]);
							ImGui::MenuItem("Render data debug", "", &m_show[Show::RenderDataDbg]);
							ImGui::MenuItem("Indexed buffer debug", "", &m_show[Show::IndexedBufferMgrDbg]);
							ImGui::EndMenu();
						}

#if defined(SHOW_IMGUI_DEMO_WINDOW)
						ImGui::Separator();
						ImGui::MenuItem("Show ImGui demo", "", &m_show[Show::ImGuiDemo]);
#endif

						ImGui::EndMenu();
					}

					if (ImGui::BeginMenu("Debug"))
					{
						m_debugGraphicsService.PopulateImGuiMenu();

						ImGui::Separator();
						
						m_cullingGraphicsService.PopulateImGuiMenu();

						ImGui::EndMenu();
					}

					if (ImGui::BeginMenu("Capture"))
					{
						if (ImGui::MenuItem("Performance overlay", "", &m_show[Show::PerfLogger]))
						{
							core::EventManager::Get()->Notify(core::EventManager::EventInfo{
								.m_eventKey = eventkey::TogglePerformanceTimers,
								.m_data = m_show[PerfLogger],
							});
						}
						ImGui::MenuItem("GPU Captures", "", &m_show[Show::GPUCaptures]);

						// TODO...
						ImGui::TextDisabled("Save screenshot");

						ImGui::EndMenu();
					}
				}
				ImGui::EndMainMenuBar();
			};
		if (m_imguiMenuActive)
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

				core::Logger::Get()->ShowImGuiWindow(&m_show[Show::Logger]);
			};
		if (m_show[Show::Logger])
		{
			m_debugUICommandMgr->Enqueue(frameNum, ShowConsoleLog);
		}

		// Scene manager debug:
		auto ShowSceneMgrDebug = [&]()
			{
				pr::SceneManager::Get()->ShowImGuiWindow(&m_show[Show::SceneMgrDbg]);
			};
		if (m_show[Show::SceneMgrDbg])
		{
			m_debugUICommandMgr->Enqueue(frameNum, ShowSceneMgrDebug);
		}

		// Entity manager debug:
		auto ShowEntityMgrDebug = [&]()
			{
				pr::EntityManager::Get()->ShowSceneObjectsImGuiWindow(&m_show[Show::EntityMgrDbg]);
				pr::EntityManager::Get()->ShowSceneTransformImGuiWindow(&m_show[Show::TransformationHierarchyDbg]);
				pr::EntityManager::Get()->ShowImGuiEntityComponentDebug(&m_show[Show::EntityComponentDbg]);
			};
		if (m_show[Show::EntityMgrDbg] || m_show[Show::TransformationHierarchyDbg] || m_show[Show::EntityComponentDbg])
		{
			m_debugUICommandMgr->Enqueue(frameNum, ShowEntityMgrDebug);
		}

		// Performance logger:
		if (m_show[Show::PerfLogger])
		{
			m_debugUICommandMgr->Enqueue(frameNum,
				[&]()
				{
					core::PerfLogger::Get()->ShowImGuiWindow(&m_show[Show::PerfLogger]);
				});
		}

		// Render manager debug:
		auto ShowRenderMgrDebug = [&]()
			{
				ImGui::SetNextWindowSize(ImVec2(
					static_cast<float>(windowWidth) * 0.25f,
					static_cast<float>(windowHeight - menuBarSize[1])),
					ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowPos(ImVec2(0, menuBarSize[1]), ImGuiCond_FirstUseEver, ImVec2(0, 0));

				gr::RenderManager::Get()->ShowRenderSystemsImGuiWindow(&m_show[Show::RenderMgrDbg]);
				gr::RenderManager::Get()->ShowRenderDataImGuiWindow(&m_show[Show::RenderDataDbg]);
				gr::RenderManager::Get()->ShowIndexedBufferManagerImGuiWindow(&m_show[Show::IndexedBufferMgrDbg]);
				gr::RenderManager::Get()->ShowGPUCapturesImGuiWindow(&m_show[Show::GPUCaptures]);
				
			};
		if (m_show[Show::RenderMgrDbg] ||
			m_show[Show::RenderDataDbg] ||
			m_show[Show::IndexedBufferMgrDbg] ||
			m_show[Show::GPUCaptures])
		{
			m_debugUICommandMgr->Enqueue(frameNum, ShowRenderMgrDebug);
		}


		// Show the ImGui demo window for debugging reference
#if defined(SHOW_IMGUI_DEMO_WINDOW)
		auto ShowImGuiDemo = [&]()
			{
				ImGui::SetNextWindowPos(
					ImVec2(static_cast<float>(windowWidth) * 0.25f, menuBarSize[1]), ImGuiCond_FirstUseEver, ImVec2(0, 0));
				ImGui::ShowDemoWindow(&m_show[Show::ImGuiDemo]);
			};
		if (m_show[Show::ImGuiDemo])
		{
			m_debugUICommandMgr->Enqueue(frameNum, ShowImGuiDemo);
		}
#endif
	}
}