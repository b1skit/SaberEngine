// © 2023 Adam Badke. All rights reserved.
#include "Batch.h"
#include "GraphicsSystem_TempDebug.h"
#include "Material.h"
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

		std::shared_ptr<re::Shader> helloTriangleShader = re::Shader::Create("HelloTriangle");

		m_helloTriangleMaterial = en::SceneManager::GetSceneData()->GetMaterial("MissingMaterial");
		m_helloTriangleMaterial->SetShader(helloTriangleShader);

		m_helloTriangle->SetMeshMaterial(m_helloTriangleMaterial);


		std::shared_ptr<re::Shader> debugShader = re::Shader::Create("Debug");


//#define TEST_STAGE_SHADER // Forces EVERYTHING to use the same shader. 
#ifdef TEST_STAGE_SHADER
		m_tempDebugStage.SetStageShader(debugShader);
#endif

		// "Set" the targets:
		m_tempDebugStage.SetTextureTargetSet(nullptr); // Render directly to the backbuffer

		gr::PipelineState defaultPipelineState;
		m_tempDebugStage.SetStagePipelineState(defaultPipelineState);

		// Add param blocks:
		m_tempDebugStage.AddPermanentParameterBlock(SceneManager::GetSceneData()->GetMainCamera()->GetCameraParams());

		pipeline.AppendRenderStage(&m_tempDebugStage);
	}


	void TempDebugGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		CreateBatches();
	}


	void TempDebugGraphicsSystem::CreateBatches()
	{
		// Hello triangle batch:
		Batch helloTriangleBatch = Batch(m_helloTriangle.get(), m_helloTriangle->GetMeshMaterial());

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
		helloTriangleBatch.SetParameterBlock(instancedMeshParams);

		m_tempDebugStage.AddBatch(helloTriangleBatch);


		// Copy the scene batches, and attach a shader:
		std::vector<re::Batch> const& sceneBatches = re::RenderManager::Get()->GetSceneBatches();

		std::shared_ptr<re::Shader> debugShader = en::SceneManager::GetSceneData()->GetShader("Debug");

		for (re::Batch const& batch : sceneBatches)
		{
			// Debug: We copy the batch to make sure all of the PBs are copied as well (E.g. instanced mesh params)
			// Then, we set a shader (as the incoming material doesn't have one)
			re::Batch batchCopy = Batch(batch);
			batchCopy.SetShader(debugShader.get());
			m_tempDebugStage.AddBatch(batchCopy);
		}
	}


	std::shared_ptr<re::TextureTargetSet const> TempDebugGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_tempDebugStage.GetTextureTargetSet();
	}
}