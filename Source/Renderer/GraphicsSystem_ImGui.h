// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"
#include "RLibrary_ImGui_Platform.h"


namespace gr
{
	class ImGuiGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<ImGuiGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "ImGui"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(ImGuiGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(ImGuiGraphicsSystem, PreRender))
			);
		}

		void RegisterInputs() override { /*No inputs*/ }
		void RegisterOutputs() override { /*No outputs*/ }


	public:
		ImGuiGraphicsSystem(gr::GraphicsSystemManager*);

		~ImGuiGraphicsSystem() = default;

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void PreRender();


	public:
		// UI Manager setup interface:
		core::FrameIndexedCommandManager* GetFrameIndexedCommandManager(); // Thread-safe ImGui command submission

		std::mutex& GetGlobalImGuiMutex() const; // Synchronize ImGui IO accesses across threads


	private:
		static constexpr size_t k_imGuiCommandBufferSize = 8 * 1024 * 1024;
		core::FrameIndexedCommandManager m_perFrameCommands;

		mutable std::mutex m_imGuiGlobalMutex;


	private:
		std::shared_ptr<gr::Stage> m_imguiLibraryStage;
	};


	inline core::FrameIndexedCommandManager* ImGuiGraphicsSystem::GetFrameIndexedCommandManager()
	{
		return &m_perFrameCommands;
	}


	inline std::mutex& ImGuiGraphicsSystem::GetGlobalImGuiMutex() const
	{
		return m_imGuiGlobalMutex;
	}
}