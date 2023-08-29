// © 2023 Adam Badke. All rights reserved.
#include "Batch.h"
#include "GraphicsSystem_TempDebug.h"
#include "Material.h"
#include "RenderManager.h"
#include "SceneManager.h"

using re::Batch;
using en::SceneManager;

//#define TEST_STAGE_SHADER // Forces EVERYTHING to use the same shader. 

namespace gr
{
	TempDebugGraphicsSystem::TempDebugGraphicsSystem(std::string name)
		: GraphicsSystem(name)
		, NamedObject(name)
		, m_helloTriangle(nullptr)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_tempDebugStage = re::RenderStage::CreateGraphicsStage("DX12 temp debug stage", gfxStageParams);
	}


	void TempDebugGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		std::shared_ptr<re::Shader> debugShader = re::Shader::Create("Debug");

#ifdef TEST_STAGE_SHADER
		m_tempDebugStage->SetStageShader(debugShader);
#endif

		// "Set" the targets:
		m_tempDebugStage->SetTextureTargetSet(nullptr); // Render directly to the backbuffer

		gr::PipelineState debugPipelineState;
		debugPipelineState.SetClearTarget(gr::PipelineState::ClearTarget::ColorDepth);
		m_tempDebugStage->SetStagePipelineState(debugPipelineState);

		// Add param blocks:
		m_tempDebugStage->AddPermanentParameterBlock(SceneManager::Get()->GetMainCamera()->GetCameraParams());

		pipeline.AppendRenderStage(m_tempDebugStage);
	}


	void TempDebugGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		CreateBatches();
	}


	void TempDebugGraphicsSystem::CreateBatches()
	{
		// Copy the scene batches, and attach a shader:
		std::vector<re::Batch> const& sceneBatches = re::RenderManager::Get()->GetSceneBatches();

		std::shared_ptr<re::Shader> debugShader = en::SceneManager::GetSceneData()->GetShader("Debug");

		for (re::Batch const& batch : sceneBatches)
		{
			// Debug: We copy the batch to make sure all of the PBs are copied as well (E.g. instanced mesh params)
			// Then, we set a shader (as the incoming material doesn't have one)
			re::Batch batchCopy = Batch(batch);
			batchCopy.SetShader(debugShader.get());
			m_tempDebugStage->AddBatch(batchCopy);
		}
	}


	std::shared_ptr<re::TextureTargetSet const> TempDebugGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_tempDebugStage->GetTextureTargetSet();
	}
}