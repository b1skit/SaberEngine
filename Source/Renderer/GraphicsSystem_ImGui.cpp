// © 2024 Adam Badke. All rights reserved.
#include "GraphicsSystem_ImGui.h"
#include "GraphicsSystemManager.h"
#include "RenderSystem.h"


namespace gr
{
	ImGuiGraphicsSystem::ImGuiGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_perFrameCommands(k_imGuiCommandBufferSize, owningGSM->GetNumFramesInFlight())
	{
	}


	void ImGuiGraphicsSystem::InitPipeline(
		gr::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&, DataDependencies const&)
	{
		// Create a library stage:
		gr::Stage::LibraryStageParams imGuiLibraryParams(
			gr::Stage::Type::LibraryRaster,
			gr::Stage::LibraryStageParams::LibraryType::ImGui);
		m_imguiLibraryStage = gr::Stage::CreateLibraryStage("ImGui stage", imGuiLibraryParams);

		// Append the library stage
		pipeline.AppendStage(m_imguiLibraryStage);
	}


	void ImGuiGraphicsSystem::PreRender()
	{
		std::unique_ptr<platform::RLibraryImGui::Payload> framePayload =
			std::make_unique<platform::RLibraryImGui::Payload>();

		framePayload->m_currentFrameNum = m_graphicsSystemManager->GetCurrentRenderFrameNum();
		framePayload->m_perFrameCommands = &m_perFrameCommands;

		dynamic_cast<gr::LibraryStage*>(m_imguiLibraryStage.get())->SetPayload(std::move(framePayload));
	}


	// ---


	void CreateAddImGuiRenderSystem::Execute(void* cmdData)
	{
		CreateAddImGuiRenderSystem* cmdPtr = reinterpret_cast<CreateAddImGuiRenderSystem*>(cmdData);

		cmdPtr->GetRenderSystemsForModification().emplace_back(gr::RenderSystem::Create(
			k_debugUIPipelineFilename,
			&cmdPtr->GetRenderData(),
			cmdPtr->GetContextForModification()));

		gr::GraphicsSystemManager const& gsm = cmdPtr->GetRenderSystemsForModification().back()->GetGraphicsSystemManager();

		gr::ImGuiGraphicsSystem* debugUIGraphicsSystem = gsm.GetGraphicsSystem<gr::ImGuiGraphicsSystem>();

		*cmdPtr->m_cmdMgrPtr = debugUIGraphicsSystem->GetFrameIndexedCommandManager();
		*cmdPtr->m_imguiMutexPtr = &debugUIGraphicsSystem->GetGlobalImGuiMutex();

		cmdPtr->m_createdFlag->store(true);
	}
}