#include <memory>

#include "GraphicsSystem_Tonemapping.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "Shader.h"
#include "Config.h"
#include "SceneManager.h"
#include "RenderManager.h"

using en::Config;
using en::SceneManager;
using gr::Shader;
using gr::DeferredLightingGraphicsSystem;
using gr::TextureTargetSet;
using re::RenderManager;
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
		m_screenAlignedQuad = meshfactory::CreateFullscreenQuad(meshfactory::ZLocation::Near);
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

		m_tonemappingStage.GetStageShader() = make_shared<Shader>(Config::Get()->GetValue<string>("toneMapShader"));
		m_tonemappingStage.GetStageShader()->Create();
		
		// Set shader constants:
		m_tonemappingStage.GetStageShader()->SetUniform(
			"exposure",
			&SceneManager::GetSceneData()->GetMainCamera()->GetExposure(),
			platform::Shader::UniformType::Float,
			1);

		m_tonemappingStage.GetStageCamera() = SceneManager::GetSceneData()->GetMainCamera();
		m_tonemappingStage.GetTextureTargetSet() = *RenderManager::Get()->GetDefaultTextureTargetSet();

		pipeline.AppendRenderStage(m_tonemappingStage);
	}


	void TonemappingGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		m_tonemappingStage.InitializeForNewFrame();
		CreateBatches();

		TextureTargetSet& deferredLightTextureTargetSet =
			RenderManager::Get()->GetGraphicsSystem<DeferredLightingGraphicsSystem>()->GetFinalTextureTargetSet();

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