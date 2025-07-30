// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsService_Culling.h"
#include "GraphicsService_Debug.h"

#include "Core/Interfaces/IEventListener.h"
#include "Core/Interfaces/IEngineComponent.h"


namespace gr
{
	class RenderManager;
}
namespace host
{
	class Window;
}
namespace pr
{
	class EntityManager;
	class SceneManager;


	class UIManager : public virtual en::IEngineComponent, public virtual core::IEventListener
	{
	public:
		UIManager(pr::SceneManager*, pr::EntityManager*, gr::RenderManager*);
		~UIManager() = default;


	public: // IEngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(uint64_t frameNum, double stepTimeMs) override;

		void HandleEvents() override;


	public:
		void SetWindow(host::Window*);


	private:
		void SubmitImGuiRenderCommands(uint64_t frameNum);

		std::atomic<bool> m_debugUIRenderSystemCreated; // True if m_debugUICommandMgr, m_imguiGlobalMutex are safe to use
		core::FrameIndexedCommandManager* m_debugUICommandMgr;
		std::mutex* m_imguiGlobalMutex;


	private:
		bool m_showImGui;
		bool m_imguiMenuActive;
		bool m_prevImguiMenuActive;

		bool m_imguiWantsToCaptureKeyboard;
		bool m_imguiWantsToCaptureMouse;
		bool m_imguiWantsTextInput;

		enum Show : uint8_t
		{
			EntityComponentDbg,
			EntityMgrDbg,
			IndexedBufferMgrDbg,
			GPUCaptures,
			ImGuiDemo,
			Logger,
			PerfLogger,
			RenderDataDbg,
			RenderMgrDbg,
			SceneMgrDbg,
			TransformationHierarchyDbg,

			Show_Count
		};
		std::array<bool, Show::Show_Count> m_show;

	private:
		bool m_vsyncState;


	private:
		host::Window* m_window;
		pr::SceneManager* m_sceneManager;
		pr::EntityManager* m_entityManager;
		gr::RenderManager* m_renderManager;


	private: // Graphics services:
		pr::CullingGraphicsService m_cullingGraphicsService;
		pr::GraphicsService_Debug m_debugGraphicsService;
	};
}