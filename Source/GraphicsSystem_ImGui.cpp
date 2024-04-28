// © 2024 Adam Badke. All rights reserved.
#include "Core\Config.h"
#include "GraphicsSystem_ImGui.h"
#include "RenderManager.h"


namespace gr
{
	constexpr char const* k_gsName = "ImGui Graphics System";


	ImGuiGraphicsSystem::ImGuiGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(k_gsName, owningGSM)
		, INamedObject(k_gsName)
		, m_perFrameCommands(k_imGuiCommandBufferSize, re::RenderManager::GetNumFramesInFlight())
	{
	}


	void ImGuiGraphicsSystem::InitPipeline(re::StagePipeline& pipeline, TextureDependencies const& texDependencies)
	{
		// Create a library stage:
		re::RenderStage::LibraryStageParams imGuiLibraryParams(re::RenderStage::LibraryStageParams::LibraryType::ImGui);
		m_imguiLibraryStage = re::RenderStage::CreateLibraryStage("ImGui stage", imGuiLibraryParams);

		// Append the library stage
		pipeline.AppendRenderStage(m_imguiLibraryStage);
	}


	void ImGuiGraphicsSystem::PreRender(DataDependencies const&)
	{
		std::unique_ptr<platform::RLibraryImGui::Payload> framePayload =
			std::make_unique<platform::RLibraryImGui::Payload>();

		framePayload->m_currentFrameNum = re::RenderManager::Get()->GetCurrentRenderFrameNum();
		framePayload->m_perFrameCommands = &m_perFrameCommands;

		dynamic_cast<re::LibraryStage*>(m_imguiLibraryStage.get())->SetPayload(std::move(framePayload));
	}
}