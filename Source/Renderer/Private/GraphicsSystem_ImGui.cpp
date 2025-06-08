// © 2024 Adam Badke. All rights reserved.
#include "Core/Config.h"
#include "Private/GraphicsSystem_ImGui.h"
#include "Private/RenderManager.h"


namespace gr
{
	ImGuiGraphicsSystem::ImGuiGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_perFrameCommands(k_imGuiCommandBufferSize, re::RenderManager::GetNumFramesInFlight())
	{
	}


	void ImGuiGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&, DataDependencies const&)
	{
		// Create a library stage:
		re::Stage::LibraryStageParams imGuiLibraryParams(
			re::Stage::Type::LibraryGraphics,
			re::Stage::LibraryStageParams::LibraryType::ImGui);
		m_imguiLibraryStage = re::Stage::CreateLibraryStage("ImGui stage", imGuiLibraryParams);

		// Append the library stage
		pipeline.AppendStage(m_imguiLibraryStage);
	}


	void ImGuiGraphicsSystem::PreRender()
	{
		std::unique_ptr<platform::RLibraryImGui::Payload> framePayload =
			std::make_unique<platform::RLibraryImGui::Payload>();

		framePayload->m_currentFrameNum = re::RenderManager::Get()->GetCurrentRenderFrameNum();
		framePayload->m_perFrameCommands = &m_perFrameCommands;

		dynamic_cast<re::LibraryStage*>(m_imguiLibraryStage.get())->SetPayload(std::move(framePayload));
	}
}