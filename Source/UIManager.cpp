// © 2023 Adam Badke. All rights reserved.
#include "Config.h"
#include "CoreEngine.h"
#include "EntityManager.h"
#include "UIManager.h"
#include "LogManager.h"
#include "RenderManager.h"


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
	{
	}


	void UIManager::Startup()
	{
		LOG("UI manager starting...");

		// Event subscriptions:
		en::EventManager::Get()->Subscribe(en::EventManager::EventType::InputToggleConsole, this);
	}


	void UIManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		HandleEvents();

		// Handle the ImGui visible/not visible and relative mouse mode toggle
		if (m_imguiMenuVisible != m_prevImguiMenuVisible)
		{
			m_prevImguiMenuVisible = m_imguiMenuVisible;

			// If true, hide the mouse and lock it to the window
			const bool captureMouse = !m_imguiMenuVisible;
			en::CoreEngine::Get()->GetWindow()->SetRelativeMouseMode(captureMouse);

			// Disable ImGui mouse listening if the console is not active: Prevents UI elements
			// flashing as the hidden mouse cursor passes by
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

		UpdateImGuiMenus();
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
			default:
				break;
			}
		}
	}


	void UIManager::SendEvent(en::EventManager::EventType eventType) const
	{
		en::EventManager::Get()->Notify(en::EventManager::EventInfo{
				.m_type = eventType,
				//.m_data0 = ,
				//.m_data1 = 
			});
	}


	void UIManager::UpdateImGuiMenus()
	{
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