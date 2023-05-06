// © 2023 Adam Badke. All rights reserved.
#include "Batch.h"
#include "GraphicsSystem_TempDebug.h"
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
		std::shared_ptr<re::Shader> helloShader = re::Shader::Create("HelloTriangle");
		m_helloTriangle->GetMeshMaterial()->SetShader(helloShader);

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


	std::shared_ptr<re::TextureTargetSet> TempDebugGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_tempDebugStage.GetTextureTargetSet();
	}


	void TempDebugGraphicsSystem::CreateBatches()
	{
		const Batch debugBatch = Batch(m_helloTriangle.get(), m_helloTriangle->GetMeshMaterial());
		m_tempDebugStage.AddBatch(debugBatch);
	}
}