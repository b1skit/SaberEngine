// © 2023 Adam Badke. All rights reserved.
#include "Batch.h"
#include "GraphicsSystem_TempDebug.h"
#include "RenderManager.h"
#include "SceneManager.h"

using re::Batch;
using en::SceneManager;


namespace gr
{
	TempDebugGraphicsSystem::TempDebugGraphicsSystem(std::string name)
		: GraphicsSystem(name)
		, NamedObject(name)
		, m_tempDebugStage("DX12 temp debug stage")
		, m_helloTriangle(nullptr)
	{
	}


	void TempDebugGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		// Debug mesh:
		m_helloTriangle = meshfactory::CreateHelloTriangle(10.f, -10.f);

		// Attach a shader:
		std::shared_ptr<re::Shader> debugShader = re::Shader::Create("HelloTriangle");

#define TEST_STAGE_SHADER
#ifdef TEST_STAGE_SHADER
		m_tempDebugStage.SetStageShader(debugShader);
#else
		m_helloTriangle->GetMeshMaterial()->SetShader(helloShader);
#endif
		

		// "Set" the targets:
		m_tempDebugStage.SetTextureTargetSet(nullptr); // Render directly to the backbuffer

		gr::PipelineState defaultPipelineState;
		m_tempDebugStage.SetStagePipelineState(defaultPipelineState);

		// Add param blocks:
		m_tempDebugStage.AddPermanentParameterBlock(SceneManager::GetSceneData()->GetMainCamera()->GetCameraParams());
		// TODO: Batch transform PB?


		pipeline.AppendRenderStage(&m_tempDebugStage);
	}


	void TempDebugGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		CreateBatches();
	}


	void TempDebugGraphicsSystem::CreateBatches()
	{
		Batch debugBatch = Batch(m_helloTriangle.get(), m_helloTriangle->GetMeshMaterial());

		std::vector<re::Batch::InstancedMeshParams> instancedMeshPBData;
		instancedMeshPBData.reserve(1);
		instancedMeshPBData.emplace_back(re::Batch::InstancedMeshParams
			{
				.g_model{glm::identity<glm::mat4>()}
			});

		std::shared_ptr<re::ParameterBlock> instancedMeshParams = re::ParameterBlock::CreateFromArray(
			re::Batch::InstancedMeshParams::s_shaderName,
			instancedMeshPBData.data(),
			sizeof(re::Batch::InstancedMeshParams),
			1,
			re::ParameterBlock::PBType::SingleFrame); // TODO: SingleFrame PB destruction needs to be deferred
		debugBatch.AddBatchParameterBlock(instancedMeshParams);

		m_tempDebugStage.AddBatch(debugBatch);

		m_tempDebugStage.AddBatches(re::RenderManager::Get()->GetSceneBatches());
	}


	std::shared_ptr<re::TextureTargetSet const> TempDebugGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_tempDebugStage.GetTextureTargetSet();
	}
}