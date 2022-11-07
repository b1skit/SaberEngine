#include <memory>

#include "GraphicsSystem_Tonemapping.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "Shader.h"
#include "CoreEngine.h"

using en::CoreEngine;
using gr::Shader;
using gr::DeferredLightingGraphicsSystem;
using gr::TextureTargetSet;
using re::Batch;
using std::shared_ptr;
using std::make_shared;
using std::string;
using glm::vec3;


namespace gr
{
	TonemappingGraphicsSystem::TonemappingGraphicsSystem(std::string name) : GraphicsSystem(name), NamedObject(name),
		m_tonemappingStage("Tonemapping stage")
	{
		m_screenAlignedQuad = gr::meshfactory::CreateFullscreenQuad(true);
	}


	void TonemappingGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		RenderStage::RenderStageParams tonemappingStageParam;
		tonemappingStageParam.m_targetClearMode	= platform::Context::ClearTarget::None;
		tonemappingStageParam.m_faceCullingMode	= platform::Context::FaceCullingMode::Back;
		tonemappingStageParam.m_srcBlendMode	= platform::Context::BlendMode::One;
		tonemappingStageParam.m_dstBlendMode	= platform::Context::BlendMode::Zero;
		tonemappingStageParam.m_depthTestMode	= platform::Context::DepthTestMode::Always;

		m_tonemappingStage.SetRenderStageParams(tonemappingStageParam);

		m_tonemappingStage.GetStageShader() = make_shared<Shader>(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("toneMapShader"));
		m_tonemappingStage.GetStageShader()->Create();
		
		// Set shader constants:
		m_tonemappingStage.GetStageShader()->SetUniform(
			"exposure",
			&CoreEngine::GetSceneManager()->GetSceneData()->GetMainCamera()->GetExposure(),
			platform::Shader::UniformType::Float,
			1);

		m_tonemappingStage.GetStageCamera() = CoreEngine::GetSceneManager()->GetSceneData()->GetMainCamera();
		m_tonemappingStage.GetTextureTargetSet() = *CoreEngine::GetRenderManager()->GetDefaultTextureTargetSet();

		pipeline.AppendRenderStage(m_tonemappingStage);
	}


	void TonemappingGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		m_tonemappingStage.InitializeForNewFrame();
		CreateBatches();

		TextureTargetSet& deferredLightTextureTargetSet =
			CoreEngine::GetRenderManager()->GetGraphicsSystem<DeferredLightingGraphicsSystem>()->GetFinalTextureTargetSet();

		m_tonemappingStage.SetTextureInput(
			"GBufferAlbedo",
			deferredLightTextureTargetSet.ColorTarget(0).GetTexture(),
			Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear));
	}


	void TonemappingGraphicsSystem::CreateBatches()
	{
		const Batch fullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr, nullptr);
		m_tonemappingStage.AddBatch(fullscreenQuadBatch);
	}
}