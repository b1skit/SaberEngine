// © 2023 Adam Badke. All rights reserved.
#include "GraphicsSystem_Debug.h"
#include "SceneManager.h"


namespace gr
{
	constexpr char const* k_gsName = "Debug Graphics System";

	DebugGraphicsSystem::DebugGraphicsSystem()
		: GraphicsSystem(k_gsName)
		, NamedObject(k_gsName)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_debugStage = re::RenderStage::CreateGraphicsStage("Debug stage", gfxStageParams);
	}


	void DebugGraphicsSystem::Create(re::StagePipeline& stagePipeline)
	{
		re::PipelineState debugPipelineState;
		debugPipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Disabled);
		debugPipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Always);

		m_debugStage->SetTextureTargetSet(nullptr); // Write directly to the swapchain backbuffer

		m_debugStage->AddPermanentParameterBlock(en::SceneManager::Get()->GetMainCamera()->GetCameraParams());
	}


	void DebugGraphicsSystem::PreRender()
	{
		CreateBatches();
	}


	void DebugGraphicsSystem::CreateBatches()
	{
		for (std::shared_ptr<gr::Mesh> mesh : en::SceneManager::GetSceneData()->GetMeshes())
		{
			gr::Bounds const& bounds = mesh->GetBounds();

			// TEMP HAX: Just draw a line through the bounds...

			
		}

		//m_debugStage->AddBatch();
	}


	//void DebugGraphicsSystem::ShowImGuiWindow()
	//{
	//	// TODO
	//}
}