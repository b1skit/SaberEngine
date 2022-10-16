#include <memory>

#include "GraphicsSystem_Tonemapping.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "Shader.h"
#include "CoreEngine.h"

using en::CoreEngine;
using gr::Shader;
using gr::DeferredLightingGraphicsSystem;
using gr::TextureTargetSet;
using std::shared_ptr;
using std::make_shared;
using std::string;
using glm::vec3;


namespace gr
{
	TonemappingGraphicsSystem::TonemappingGraphicsSystem(std::string name) : GraphicsSystem(name), NamedObject(name),
		m_tonemappingStage("Tonemapping stage")
	{
	}


	void TonemappingGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		m_screenAlignedQuad.reserve(1);  // MUST reserve so our pointers won't change
		m_screenAlignedQuad.emplace_back(gr::meshfactory::CreateQuad
		(
			vec3(-1.0f, 1.0f, 0.0f),	// TL
			vec3(1.0f, 1.0f, 0.0f),		// TR
			vec3(-1.0f, -1.0f, 0.0f),	// BL
			vec3(1.0f, -1.0f, 0.0f)		// BR
		));

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
		m_tonemappingStage.SetGeometryBatches(&m_screenAlignedQuad);

		TextureTargetSet& deferredLightTextureTargetSet =
			CoreEngine::GetRenderManager()->GetGraphicsSystem<DeferredLightingGraphicsSystem>()->GetFinalTextureTargetSet();

		m_tonemappingStage.SetTextureInput(
			"GBufferAlbedo",
			deferredLightTextureTargetSet.ColorTarget(0).GetTexture(),
			Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear));
	}
}