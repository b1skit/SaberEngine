// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/IEventListener.h"
#include "Core/Interfaces/IEngineComponent.h"

#include "Core/Util/ImGuiUtils.h"


namespace app
{
	class Window;
}

namespace fr
{
	class UIManager : public virtual en::IEngineComponent, public virtual core::IEventListener
	{
	public:
		static UIManager* Get(); // Singleton functionality


	public:
		UIManager();
		~UIManager() = default;


	public: // IEngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(uint64_t frameNum, double stepTimeMs) override;

		void HandleEvents() override;


	public:
		void SetWindow(app::Window*);


	private:
		void SubmitImGuiRenderCommands(uint64_t frameNum);

		std::atomic<bool> m_debugUIRenderSystemCreated; // True if m_debugUICommandMgr, m_imguiGlobalMutex are safe to use
		core::FrameIndexedCommandManager* m_debugUICommandMgr;
		std::mutex* m_imguiGlobalMutex;


	private:
		bool m_imguiMenuVisible;
		bool m_prevImguiMenuVisible;

		bool m_imguiWantsToCaptureKeyboard;
		bool m_imguiWantsToCaptureMouse;

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
		std::array<bool, Show::Show_Count> m_show;


	private:
		app::Window* m_window;
	};
}