// © 2022 Adam Badke. All rights reserved.
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystemManager.h"
#include "LightRenderData.h"
#include "Sampler.h"
#include "Texture.h"
#include "TextureTarget.h"

#include "Renderer/Shaders/Common/SkyboxParams.h"


namespace
{
	static const EffectID k_skyboxEffectID = effect::Effect::ComputeEffectID("Skybox");


	SkyboxData CreateSkyboxParamsData(glm::vec3 const& backgroundColor, bool showBackgroundColor)
	{
		SkyboxData skyboxParams;
		skyboxParams.g_backgroundColorIsEnabled = glm::vec4(backgroundColor.rgb, static_cast<float>(showBackgroundColor));
		return skyboxParams;
	}
}

namespace gr
{
	SkyboxGraphicsSystem::SkyboxGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_skyTexture(nullptr)
		, m_backgroundColor(135.f / 255.f, 206.f / 255.f, 235.f / 255.f)
		, m_showBackgroundColor(false)
		, m_isDirty(true)
	{
	}


	void SkyboxGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&, DataDependencies const&)
	{
		re::Stage::FullscreenQuadParams fsqParams;
		fsqParams.m_zLocation = gr::meshfactory::ZLocation::Far;
		fsqParams.m_effectID = k_skyboxEffectID;

		m_skyboxStage = re::Stage::CreateFullscreenQuadStage("Skybox stage", fsqParams);

		if (m_fallbackColorTex == nullptr)
		{
			re::Texture::TextureParams fallbackParams{};
			fallbackParams.m_usage = 
				static_cast<re::Texture::Usage>(re::Texture::Usage::ColorSrc | re::Texture::Usage::ColorTarget),
			fallbackParams.m_dimension = re::Texture::Dimension::Texture2D;
			fallbackParams.m_format = re::Texture::Format::RGBA32F; // Same as an IBl, for consistency
			fallbackParams.m_colorSpace = re::Texture::ColorSpace::Linear;
			fallbackParams.m_mipMode = re::Texture::MipMode::AllocateGenerate;
			fallbackParams.m_multisampleMode = re::Texture::MultisampleMode::Disabled;

			m_fallbackColorTex = 
				re::Texture::Create("Skybox flat color fallback", fallbackParams, glm::vec4(m_backgroundColor.rgb, 1.f));
		}

		m_skyboxStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());

		// Create a new texture target set so we can write to the deferred lighting color targets, but attach the
		// GBuffer depth for HW depth testing
		std::shared_ptr<re::TextureTargetSet> skyboxTargets = re::TextureTargetSet::Create("Skybox Targets");

		skyboxTargets->SetColorTarget(
			0, 
			*texDependencies.at(k_skyboxTargetInput),
			re::TextureTarget::TargetParams{ .m_textureView = re::TextureView::Texture2DView(0, 1) });

		re::TextureTarget::TargetParams depthTargetParams{ .m_textureView = {
				re::TextureView::Texture2DView(0, 1), 
				re::TextureView::ViewFlags{re::TextureView::ViewFlags::ReadOnlyDepth}} };
		
		skyboxTargets->SetDepthStencilTarget(*texDependencies.at(k_sceneDepthTexInput), depthTargetParams);

		m_skyboxStage->SetTextureTargetSet(skyboxTargets);

		m_skyboxParams = re::Buffer::Create(
			SkyboxData::s_shaderName,
			CreateSkyboxParamsData(m_backgroundColor, m_showBackgroundColor),
			re::Buffer::BufferParams{
				.m_stagingPool = re::Buffer::StagingPool::Permanent,
				.m_memPoolPreference = re::Buffer::UploadHeap,
				.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
				.m_usageMask = re::Buffer::Constant,
			});

		m_skyboxStage->AddPermanentBuffer(SkyboxData::s_shaderName, m_skyboxParams);

		// Start with our default texture set, in case there is no IBL
		m_skyTexture = &m_fallbackColorTex;

		m_skyboxStage->AddPermanentTextureInput(
			k_skyboxTexShaderName,
			*m_skyTexture,
			re::Sampler::GetSampler("WrapMinMagLinearMipPoint"),
			re::TextureView(*m_skyTexture));


		pipeline.AppendStage(m_skyboxStage);
	}


	void SkyboxGraphicsSystem::RegisterInputs()
	{
		RegisterTextureInput(k_skyboxTargetInput);
		RegisterTextureInput(k_sceneDepthTexInput);
	}


	void SkyboxGraphicsSystem::RegisterOutputs()
	{
		//
	}


	void SkyboxGraphicsSystem::PreRender()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		if (m_graphicsSystemManager->ActiveAmbientLightHasChanged())
		{
			if (m_graphicsSystemManager->HasActiveAmbientLight())
			{
				const gr::RenderDataID ambientID = m_graphicsSystemManager->GetActiveAmbientLightID();

				gr::Light::RenderDataAmbientIBL const& ambientRenderData =
					renderData.GetObjectData<gr::Light::RenderDataAmbientIBL>(ambientID);

				m_skyTexture = &ambientRenderData.m_iblTex;
			}
			else
			{
				m_skyTexture = &m_fallbackColorTex;
			}

			m_skyboxStage->AddPermanentTextureInput(
				k_skyboxTexShaderName,
				*m_skyTexture,
				re::Sampler::GetSampler("WrapMinMagLinearMipPoint"),
				re::TextureView(*m_skyTexture));

		}
		SEAssert(m_skyTexture != nullptr, "Failed to set a valid sky texture");

		if (m_isDirty)
		{
			m_skyboxParams->Commit(CreateSkyboxParamsData(m_backgroundColor, m_showBackgroundColor));
			m_isDirty = false;
		}
	}


	void SkyboxGraphicsSystem::ShowImGuiWindow()
	{
		m_isDirty |= ImGui::Checkbox("Use flat background color", &m_showBackgroundColor);
		m_isDirty |= ImGui::ColorEdit3("Background color", &m_backgroundColor.r);
	}
}