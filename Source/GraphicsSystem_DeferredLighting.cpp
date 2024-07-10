// © 2022 Adam Badke. All rights reserved.
#include "Buffer.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystemManager.h"
#include "LightManager.h"
#include "LightParamsHelpers.h"
#include "LightRenderData.h"
#include "MeshFactory.h"
#include "Sampler.h"
#include "ShadowMapRenderData.h"
#include "RenderDataManager.h"
#include "RenderManager.h"
#include "RenderStage.h"

#include "Core/Config.h"

#include "Shaders/Common/IBLGenerationParams.h"
#include "Shaders/Common/LightParams.h"


namespace
{
	BRDFIntegrationData GetBRDFIntegrationParamsDataData()
	{
		const uint32_t brdfTexWidthHeight =
			static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey));

		BRDFIntegrationData brdfIntegrationParams{
			.g_integrationTargetResolution =
				glm::uvec4(brdfTexWidthHeight, brdfTexWidthHeight, 0, 0)
		};

		return brdfIntegrationParams;
	}


	IEMPMREMGenerationData GetIEMPMREMGenerationParamsDataData(
		int currentMipLevel, int numMipLevels, uint32_t faceIdx, uint32_t srcWidth, uint32_t srcHeight)
	{
		IEMPMREMGenerationData generationParams;

		SEAssert(currentMipLevel >= 0 && numMipLevels >= 1,
			"Mip level params are invalid. These must be reasonable, even if they're not used (i.e. IEM generation)");

		float roughness;
		if (numMipLevels > 1)
		{
			roughness = static_cast<float>(currentMipLevel) / static_cast<float>(numMipLevels - 1);
		}
		else
		{
			roughness = 0;
		}

		const int numIEMSamples = core::Config::Get()->GetValue<int>(core::configkeys::k_iemNumSamplesKey);
		const int numPMREMSamples = core::Config::Get()->GetValue<int>(core::configkeys::k_pmremNumSamplesKey);

		generationParams.g_numSamplesRoughnessFaceIdx = glm::vec4(
			static_cast<float>(numIEMSamples),
			static_cast<float>(numPMREMSamples),
			roughness,
			faceIdx);

		// We sample a lower mip level to approximate a Gaussian blur of the input image (i.e. low-pass filter), 
		// significantly reducing the required number of samples required to get a noise free convolution.
		// Empirical testing shows that for N = 4096 IEM samples per pixel, a 128x64 src image gives reasonable
		// results.
		// We assume our IBL inputs are roughly 2:1 in dimensions, and compute the src mip from the maximum dimension
		const float maxDimension = static_cast<float>(std::max(srcWidth, srcHeight));
		constexpr uint32_t k_targetMaxDimension = 128;
		generationParams.g_mipLevelSrcWidthSrcHeightSrcNumMips = glm::vec4(
			glm::log2(maxDimension / k_targetMaxDimension), 
			srcWidth, 
			srcHeight, 
			numMipLevels);

		return generationParams;
	}
}


namespace gr
{
	DeferredLightingGraphicsSystem::DeferredLightingGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_resourceCreationStagePipeline(nullptr)
	{
		m_lightingTargetSet = re::TextureTargetSet::Create("Deferred light targets");
	}


	void DeferredLightingGraphicsSystem::RegisterInputs()
	{
		// Deferred lighting GS is (currently) tightly coupled to the GBuffer GS
		for (uint8_t slot = 0; slot < GBufferGraphicsSystem::GBufferColorTex_Count; slot++)
		{
			if (slot == GBufferGraphicsSystem::GBufferEmissive)
			{
				continue;
			}

			RegisterTextureInput(GBufferGraphicsSystem::GBufferTexNameHashKeys[slot]);
		}
		RegisterTextureInput(GBufferGraphicsSystem::GBufferTexNameHashKeys[GBufferGraphicsSystem::GBufferDepth]);

		RegisterTextureInput(k_ssaoInput, TextureInputDefault::OpaqueWhite);

		RegisterDataInput(k_pointLightCullingDataInput);
		RegisterDataInput(k_spotLightCullingDataInput);
		RegisterDataInput(k_shadowTexturesInput);
	}


	void DeferredLightingGraphicsSystem::RegisterOutputs()
	{
		RegisterTextureOutput(k_lightingTexOutput, &m_lightingTargetSet->GetColorTarget(0).GetTexture());
		RegisterTextureOutput(k_activeAmbientIEMTexOutput, &m_activeAmbientLightData.m_IEMTex);
		RegisterTextureOutput(k_activeAmbientPMREMTexOutput, &m_activeAmbientLightData.m_PMREMTex);
		RegisterTextureOutput(k_activeAmbientDFGTexOutput, &m_BRDF_integrationMap);
		
		RegisterBufferOutput(k_activeAmbientParamsBufferOutput, &m_activeAmbientLightData.m_ambientParams);
	}


	void DeferredLightingGraphicsSystem::CreateSingleFrameBRDFPreIntegrationStage(re::StagePipeline& pipeline)
	{
		re::RenderStage::ComputeStageParams computeStageParams;
		std::shared_ptr<re::RenderStage> brdfStage =
			re::RenderStage::CreateSingleFrameComputeStage("BRDF pre-integration compute stage", computeStageParams);

		brdfStage->SetDrawStyle(effect::DrawStyle::DeferredLighting_BRDFIntegration);

		const uint32_t brdfTexWidthHeight =
			static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey));

		// Create a render target texture:			
		re::Texture::TextureParams brdfParams;
		brdfParams.m_width = brdfTexWidthHeight;
		brdfParams.m_height = brdfTexWidthHeight;
		brdfParams.m_usage =
			static_cast<re::Texture::Usage>(re::Texture::Usage::ComputeTarget | re::Texture::Usage::Color);
		brdfParams.m_dimension = re::Texture::Dimension::Texture2D;
		brdfParams.m_format = re::Texture::Format::RGBA16F;
		brdfParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		brdfParams.m_mipMode = re::Texture::MipMode::None;
		brdfParams.m_addToSceneData = false;
		brdfParams.m_clear.m_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		m_BRDF_integrationMap = re::Texture::Create("BRDFIntegrationMap", brdfParams);

		std::shared_ptr<re::TextureTargetSet> brdfStageTargets = re::TextureTargetSet::Create("BRDF Stage Targets");

		re::TextureTarget::TargetParams colorTargetParams{.m_textureView = re::TextureView::Texture2DView(0, 1)};

		brdfStageTargets->SetColorTarget(0, m_BRDF_integrationMap, colorTargetParams);
		brdfStageTargets->SetViewport(re::Viewport(0, 0, brdfTexWidthHeight, brdfTexWidthHeight));
		brdfStageTargets->SetScissorRect(re::ScissorRect(0, 0, brdfTexWidthHeight, brdfTexWidthHeight));

		re::TextureTarget::TargetParams::BlendModes brdfBlendModes
		{
			re::TextureTarget::TargetParams::BlendMode::One,
			re::TextureTarget::TargetParams::BlendMode::Zero,
		};
		brdfStageTargets->SetColorTargetBlendModes(1, &brdfBlendModes);

		brdfStage->SetTextureTargetSet(brdfStageTargets);

		BRDFIntegrationData const& brdfIntegrationParams = GetBRDFIntegrationParamsDataData();
		std::shared_ptr<re::Buffer> brdfIntegrationBuf = re::Buffer::Create(
			BRDFIntegrationData::s_shaderName,
			brdfIntegrationParams,
			re::Buffer::Type::SingleFrame);
		brdfStage->AddSingleFrameBuffer(brdfIntegrationBuf);

		// Add our dispatch information to a compute batch. Note: We use numthreads = (1,1,1)
		re::Batch computeBatch = re::Batch(
			re::Batch::Lifetime::SingleFrame,
			re::Batch::ComputeParams{
				.m_threadGroupCount = glm::uvec3(brdfTexWidthHeight, brdfTexWidthHeight, 1u) },
			effect::Effect::ComputeEffectID("DeferredLighting"));

		brdfStage->AddBatch(computeBatch);

		pipeline.AppendSingleFrameRenderStage(std::move(brdfStage));
	}


	void DeferredLightingGraphicsSystem::PopulateIEMTex(
		re::StagePipeline* pipeline, re::Texture const* iblTex, std::shared_ptr<re::Texture>& iemTexOut) const
	{
		const uint32_t iemTexWidthHeight =
			static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_iemTexWidthHeightKey));

		// IEM-specific texture params:
		re::Texture::TextureParams iemTexParams;
		iemTexParams.m_width = iemTexWidthHeight;
		iemTexParams.m_height = iemTexWidthHeight;
		iemTexParams.m_usage = 
			static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::Color);
		iemTexParams.m_dimension = re::Texture::Dimension::TextureCube;
		iemTexParams.m_format = re::Texture::Format::RGBA16F;
		iemTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		iemTexParams.m_addToSceneData = false;
		iemTexParams.m_mipMode = re::Texture::MipMode::None;

		const std::string IEMTextureName = iblTex->GetName() + "_IEMTexture";
		iemTexOut = re::Texture::Create(IEMTextureName, iemTexParams);

		for (uint32_t face = 0; face < 6; face++)
		{
			re::RenderStage::GraphicsStageParams gfxStageParams;
			std::shared_ptr<re::RenderStage> iemStage = re::RenderStage::CreateSingleFrameGraphicsStage(
				std::format("IEM generation: Face {}/6", face + 1).c_str(), gfxStageParams);

			iemStage->SetDrawStyle(effect::DrawStyle::DeferredLighting_IEMGeneration);
			iemStage->AddPermanentTextureInput(
				"Tex0",
				iblTex,
				re::Sampler::GetSampler("WrapMinMagLinearMipPoint").get(),
				re::TextureView(iblTex));

			// Buffers:
			IEMPMREMGenerationData const& iemGenerationParams =
				GetIEMPMREMGenerationParamsDataData(0, 1, face, iblTex->Width(), iblTex->Height());
			std::shared_ptr<re::Buffer> iemGenerationBuffer = re::Buffer::Create(
				IEMPMREMGenerationData::s_shaderName,
				iemGenerationParams,
				re::Buffer::Type::SingleFrame);
			iemStage->AddSingleFrameBuffer(iemGenerationBuffer);

			iemStage->AddPermanentBuffer(m_cubemapRenderCamParams[face]);

			std::shared_ptr<re::TextureTargetSet> iemTargets = re::TextureTargetSet::Create("IEM Stage Targets");

			re::TextureTarget::TargetParams::BlendModes iemBlendModes
			{
				re::TextureTarget::TargetParams::BlendMode::One,
				re::TextureTarget::TargetParams::BlendMode::Zero,
			};
			iemTargets->SetColorTargetBlendModes(1, &iemBlendModes);

			re::TextureTarget::TargetParams targetParams{
				.m_textureView = re::TextureView::Texture2DArrayView(0, 1, face, 1)};

			iemTargets->SetColorTarget(0, iemTexOut, targetParams);
			iemTargets->SetViewport(re::Viewport(0, 0, iemTexWidthHeight, iemTexWidthHeight));
			iemTargets->SetScissorRect(re::ScissorRect(0, 0, iemTexWidthHeight, iemTexWidthHeight));

			iemStage->SetTextureTargetSet(iemTargets);

			iemStage->AddBatch(*m_cubeMeshBatch);

			pipeline->AppendSingleFrameRenderStage(std::move(iemStage));
		}
	}


	void DeferredLightingGraphicsSystem::PopulatePMREMTex(
		re::StagePipeline* pipeline, re::Texture const* iblTex, std::shared_ptr<re::Texture>& pmremTexOut) const
	{
		const uint32_t pmremTexWidthHeight =
			static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_pmremTexWidthHeightKey));

		// PMREM-specific texture params:
		re::Texture::TextureParams pmremTexParams;
		pmremTexParams.m_width = pmremTexWidthHeight;
		pmremTexParams.m_height = pmremTexWidthHeight;
		pmremTexParams.m_usage = 
			static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::Color);
		pmremTexParams.m_dimension = re::Texture::Dimension::TextureCube;
		pmremTexParams.m_format = re::Texture::Format::RGBA16F;
		pmremTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		pmremTexParams.m_addToSceneData = false;
		pmremTexParams.m_mipMode = re::Texture::MipMode::Allocate;

		const std::string PMREMTextureName = iblTex->GetName() + "_PMREMTexture";
		pmremTexOut = re::Texture::Create(PMREMTextureName, pmremTexParams);

		const uint32_t totalMipLevels = pmremTexOut->GetNumMips();

		for (uint32_t face = 0; face < 6; face++)
		{
			for (uint32_t currentMipLevel = 0; currentMipLevel < totalMipLevels; currentMipLevel++)
			{
				std::string const& postFix = std::format("Face {}, Mip {}", face, currentMipLevel);
				std::string const& stageName = std::format("PMREM generation: {}", postFix);

				re::RenderStage::GraphicsStageParams gfxStageParams;
				std::shared_ptr<re::RenderStage> pmremStage = re::RenderStage::CreateSingleFrameGraphicsStage(
					stageName.c_str(), gfxStageParams);

				pmremStage->SetDrawStyle(effect::DrawStyle::DeferredLighting_PMREMGeneration);

				pmremStage->AddPermanentTextureInput(
					"Tex0",
					iblTex,
					re::Sampler::GetSampler("ClampMinMagMipLinear").get(),
					re::TextureView(iblTex));

				// Buffers:
				IEMPMREMGenerationData const& pmremGenerationParams = GetIEMPMREMGenerationParamsDataData(
					currentMipLevel, totalMipLevels, face, iblTex->Width(), iblTex->Height());
				std::shared_ptr<re::Buffer> pmremGenerationBuffer = re::Buffer::Create(
					IEMPMREMGenerationData::s_shaderName,
					pmremGenerationParams,
					re::Buffer::Type::SingleFrame);
				pmremStage->AddSingleFrameBuffer(pmremGenerationBuffer);

				pmremStage->AddPermanentBuffer(m_cubemapRenderCamParams[face]);

				re::TextureTarget::TargetParams targetParams{ 
					.m_textureView = re::TextureView::Texture2DArrayView(currentMipLevel, 1, face, 1)};

				std::shared_ptr<re::TextureTargetSet> pmremTargetSet =
					re::TextureTargetSet::Create("PMREM texture targets: Face " + postFix);

				pmremTargetSet->SetColorTarget(0, pmremTexOut, targetParams);

				const glm::vec4 mipDimensions =
					pmremTexOut->GetMipLevelDimensions(currentMipLevel);
				const uint32_t mipWidth = static_cast<uint32_t>(mipDimensions.x);
				const uint32_t mipHeight = static_cast<uint32_t>(mipDimensions.y);

				pmremTargetSet->SetViewport(re::Viewport(0, 0, mipWidth, mipHeight));
				pmremTargetSet->SetScissorRect(re::ScissorRect(0, 0, mipWidth, mipHeight));

				re::TextureTarget::TargetParams::BlendModes pmremBlendModes
				{
					re::TextureTarget::TargetParams::BlendMode::One,
					re::TextureTarget::TargetParams::BlendMode::Zero,
				};
				pmremTargetSet->SetColorTargetBlendModes(1, &pmremBlendModes);

				pmremStage->SetTextureTargetSet(pmremTargetSet);

				pmremStage->AddBatch(*m_cubeMeshBatch);

				pipeline->AppendSingleFrameRenderStage(std::move(pmremStage));
			}
		}
	}


	void DeferredLightingGraphicsSystem::InitializeResourceGenerationStages(
		re::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&)
	{
		m_resourceCreationStagePipeline = &pipeline;

		m_resourceCreationStageParentItr = pipeline.AppendRenderStage(
			re::RenderStage::CreateParentStage("Resource creation stages parent"));


		// Cube mesh, for rendering of IBL cubemaps
		if (m_cubeMeshPrimitive == nullptr)
		{
			m_cubeMeshPrimitive = gr::meshfactory::CreateCube();
		}

		// Create a cube mesh batch, for reuse during the initial frame IBL rendering:
		if (m_cubeMeshBatch == nullptr)
		{
			m_cubeMeshBatch = std::make_unique<re::Batch>(
				re::Batch::Lifetime::Permanent,
				m_cubeMeshPrimitive.get(),
				effect::Effect::ComputeEffectID("DeferredLighting"));
		}

		// Camera render params for 6 cubemap faces; Just need to update g_view for each face/stage
		CameraData cubemapCamParams{};

		cubemapCamParams.g_projection = gr::Camera::BuildPerspectiveProjectionMatrix(
			glm::radians(90.f), // yFOV
			1.f,				// Aspect ratio
			0.1f,				// Near
			10.f);				// Far

		cubemapCamParams.g_viewProjection = glm::mat4(1.f); // Identity; unused
		cubemapCamParams.g_invViewProjection = glm::mat4(1.f); // Identity; unused
		cubemapCamParams.g_cameraWPos = glm::vec4(0.f, 0.f, 0.f, 0.f); // Unused

		std::vector<glm::mat4> const& cubemapViews = gr::Camera::BuildAxisAlignedCubeViewMatrices(glm::vec3(0.f));

		for (uint8_t face = 0; face < 6; face++)
		{
			if (m_cubemapRenderCamParams[face] == nullptr)
			{
				cubemapCamParams.g_view = cubemapViews[face];

				m_cubemapRenderCamParams[face] = re::Buffer::Create(
					CameraData::s_shaderName,
					cubemapCamParams,
					re::Buffer::Type::Immutable);
			}
		}

		// 1st frame: Generate the pre-integrated BRDF LUT via a single-frame compute stage:
		CreateSingleFrameBRDFPreIntegrationStage(pipeline);
	}


	void DeferredLightingGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&)
	{
		m_missing2DShadowFallback = re::Texture::Create("Missing 2D shadow fallback",
			re::Texture::TextureParams
			{
				.m_usage = re::Texture::Usage::Color,
				.m_dimension = re::Texture::Dimension::Texture2D,
				.m_format = re::Texture::Format::Depth32F,
				.m_colorSpace = re::Texture::ColorSpace::Linear,
				.m_mipMode = re::Texture::MipMode::None,
				.m_addToSceneData = false,
				.m_clear = re::Texture::TextureParams::ClearValues{},
			},
			glm::vec4(1.f, 1.f, 1.f, 1.f));

		m_missingCubeShadowFallback = re::Texture::Create("Missing cubemap shadow fallback", 
			re::Texture::TextureParams
			{
				.m_usage = re::Texture::Usage::Color,
				.m_dimension = re::Texture::Dimension::TextureCube,
				.m_format = re::Texture::Format::Depth32F,
				.m_colorSpace = re::Texture::ColorSpace::Linear,
				.m_mipMode = re::Texture::MipMode::None,
				.m_addToSceneData = false,
				.m_clear = re::Texture::TextureParams::ClearValues{},
			},
			glm::vec4(1.f, 1.f, 1.f, 1.f));

		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_ambientStage = re::RenderStage::CreateGraphicsStage("Ambient light stage", gfxStageParams);

		m_directionalStage = re::RenderStage::CreateGraphicsStage("Directional light stage", gfxStageParams);
		m_pointStage = re::RenderStage::CreateGraphicsStage("Point light stage", gfxStageParams);
		m_spotStage = re::RenderStage::CreateGraphicsStage("Spot light stage", gfxStageParams);

		// Create a lighting texture target:
		re::Texture::TextureParams lightTargetTexParams;
		lightTargetTexParams.m_width = core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
		lightTargetTexParams.m_height = core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);
		lightTargetTexParams.m_usage = 
			static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::Color);
		lightTargetTexParams.m_dimension = re::Texture::Dimension::Texture2D;
		lightTargetTexParams.m_format = re::Texture::Format::RGBA16F;
		lightTargetTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		lightTargetTexParams.m_mipMode = re::Texture::MipMode::None;
		lightTargetTexParams.m_addToSceneData = false;
		lightTargetTexParams.m_clear.m_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		std::shared_ptr<re::Texture> lightTargetTex = re::Texture::Create("DeferredLightTarget", lightTargetTexParams);

		// Create the lighting target set (shared by all lights/stages):
		re::TextureTarget::TargetParams deferredTargetParams{ .m_textureView = re::TextureView::Texture2DView(0, 1)};
		deferredTargetParams.m_clearMode = re::TextureTarget::TargetParams::ClearMode::Disabled;

		m_lightingTargetSet->SetColorTarget(0, lightTargetTex, deferredTargetParams);

		// We need the depth buffer attached, but with depth writes disabled:
		re::TextureTarget::TargetParams depthTargetParams{ .m_textureView = {
				re::TextureView::Texture2DView(0, 1),
				{re::TextureView::ViewFlags::ReadOnlyDepth} } };

		m_lightingTargetSet->SetDepthStencilTarget(
			*texDependencies.at(GBufferGraphicsSystem::GBufferTexNameHashKeys[GBufferGraphicsSystem::GBufferDepth]),
			depthTargetParams);

		// All deferred lighting is additive
		re::TextureTarget::TargetParams::BlendModes deferredBlendModes
		{
			re::TextureTarget::TargetParams::BlendMode::One,
			re::TextureTarget::TargetParams::BlendMode::One,
		};
		m_lightingTargetSet->SetColorTargetBlendModes(1, &deferredBlendModes);


		// Append a color-only clear stage to clear the lighting target:
		re::RenderStage::ClearStageParams colorClearParams;
		colorClearParams.m_colorClearModes = { re::TextureTarget::TargetParams::ClearMode::Enabled };
		colorClearParams.m_depthClearMode = re::TextureTarget::TargetParams::ClearMode::Disabled;
		pipeline.AppendRenderStage(re::RenderStage::CreateClearStage(colorClearParams, m_lightingTargetSet));


		// Ambient stage:
		// --------------
		m_ambientStage->SetTextureTargetSet(m_lightingTargetSet);

		m_ambientStage->SetDrawStyle(effect::DrawStyle::DeferredLighting_DeferredAmbient);

		m_ambientStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());	

		// Get/set the AO texture. If it doesn't exist, we'll get a default opaque white texture
		m_ssaoTex = *texDependencies.at(k_ssaoInput);

		std::shared_ptr<re::Sampler> clampMinMagMipPoint = re::Sampler::GetSampler("ClampMinMagMipPoint");

		m_ambientStage->AddPermanentTextureInput(
			k_ssaoInput.GetKey(), m_ssaoTex, clampMinMagMipPoint, re::TextureView(m_ssaoTex));

		// Append the ambient stage:
		pipeline.AppendRenderStage(m_ambientStage);
		

		// Common PCSS sampling params:
		PoissonSampleParamsData const& poissonSampleParamsData = GetPoissonSampleParamsData();

		std::shared_ptr<re::Buffer> poissonSampleParams = re::Buffer::Create(
			PoissonSampleParamsData::s_shaderName,
			poissonSampleParamsData,
			re::Buffer::Type::Immutable);


		// Directional light stage:
		//-------------------------
		m_directionalStage->SetTextureTargetSet(m_lightingTargetSet);
		
		m_directionalStage->SetDrawStyle(effect::DrawStyle::DeferredLighting_DeferredDirectional);

		m_directionalStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_directionalStage->AddPermanentBuffer(poissonSampleParams);

		pipeline.AppendRenderStage(m_directionalStage);


		// Point light stage:
		//-------------------
		m_pointStage->SetTextureTargetSet(m_lightingTargetSet);
		m_pointStage->AddPermanentBuffer(m_lightingTargetSet->GetCreateTargetParamsBuffer());

		m_pointStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_pointStage->AddPermanentBuffer(poissonSampleParams);

		m_pointStage->SetDrawStyle(effect::DrawStyle::DeferredLighting_DeferredPoint);

		pipeline.AppendRenderStage(m_pointStage);


		// Spot light stage:
		//------------------
		m_spotStage->SetTextureTargetSet(m_lightingTargetSet);
		m_spotStage->AddPermanentBuffer(m_lightingTargetSet->GetCreateTargetParamsBuffer());

		m_spotStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_spotStage->AddPermanentBuffer(poissonSampleParams);

		m_spotStage->SetDrawStyle(effect::DrawStyle::DeferredLighting_DeferredSpot);

		pipeline.AppendRenderStage(m_spotStage);


		// Attach GBuffer color inputs:
		std::shared_ptr<re::Sampler> wrapMinMagLinearMipPoint = re::Sampler::GetSampler("WrapMinMagLinearMipPoint");

		constexpr uint8_t numGBufferColorInputs = 
			static_cast<uint8_t>(GBufferGraphicsSystem::GBufferTexIdx::GBufferColorTex_Count);

		for (uint8_t slot = 0; slot < numGBufferColorInputs; slot++)
		{
			if (slot == GBufferGraphicsSystem::GBufferEmissive)
			{
				continue; // The emissive texture is not used
			}

			SEAssert(texDependencies.contains(GBufferGraphicsSystem::GBufferTexNameHashKeys[slot]),
				"Texture dependency not found");

			util::HashKey const& texName = GBufferGraphicsSystem::GBufferTexNameHashKeys[slot];
			std::shared_ptr<re::Texture> const& gbufferTex = *texDependencies.at(texName);
			
			re::TextureView gbufferTexView = re::TextureView(gbufferTex);

			m_ambientStage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, gbufferTexView);
			m_directionalStage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, gbufferTexView);
			m_pointStage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, gbufferTexView);
			m_spotStage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, gbufferTexView);
		}


		// Attach the GBUffer depth input:
		constexpr uint8_t depthBufferSlot = gr::GBufferGraphicsSystem::GBufferDepth;
		util::HashKey const& depthName = GBufferGraphicsSystem::GBufferTexNameHashKeys[depthBufferSlot];
		std::shared_ptr<re::Texture> const& depthTex = *texDependencies.at(depthName);

		re::TextureView gbufferDepthTexView = re::TextureView(depthTex);

		m_directionalStage->AddPermanentTextureInput(
			depthName.GetKey(), depthTex, wrapMinMagLinearMipPoint, gbufferDepthTexView);
		m_pointStage->AddPermanentTextureInput(
			depthName.GetKey(), depthTex, wrapMinMagLinearMipPoint, gbufferDepthTexView);
		m_spotStage->AddPermanentTextureInput(
			depthName.GetKey(), depthTex, wrapMinMagLinearMipPoint, gbufferDepthTexView);
		m_ambientStage->AddPermanentTextureInput(
			depthName.GetKey(), depthTex, wrapMinMagLinearMipPoint, gbufferDepthTexView);

		m_ambientStage->AddPermanentTextureInput(
			"DFG", m_BRDF_integrationMap, clampMinMagMipPoint, re::TextureView(m_BRDF_integrationMap));
	}


	void DeferredLightingGraphicsSystem::PreRender(DataDependencies const& dataDependencies)
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Removed any deleted directional/point/spot lights:
		auto DeleteLights = []<typename T>(
			std::vector<gr::RenderDataID> const* deletedIDs, std::unordered_map<gr::RenderDataID, T>&stageData)
		{
			if (!deletedIDs)
			{
				return;
			}
			for (gr::RenderDataID id : *deletedIDs)
			{
				stageData.erase(id);
			}
		};

		// Null out the active ambient light tracking if it has been deleted
		std::vector<gr::RenderDataID> const* deletedAmbientIDs =
			renderData.GetIDsWithDeletedData<gr::Light::RenderDataAmbientIBL>();
		for (auto const& deletedAmbientID : *deletedAmbientIDs)
		{
			if (deletedAmbientID == m_activeAmbientLightData.m_renderDataID)
			{
				m_activeAmbientLightData = {};
				break;
			}
		}
		DeleteLights(deletedAmbientIDs, m_ambientLightData);
		
		DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataDirectional>(), m_punctualLightData);
		DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataPoint>(), m_punctualLightData);
		DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataSpot>(), m_punctualLightData);
		

		// Register new ambient lights:
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataAmbientIBL>())
		{
			std::vector<gr::RenderDataID> const* newAmbientIDs =
				renderData.GetIDsWithNewData<gr::Light::RenderDataAmbientIBL>();

			if (newAmbientIDs)
			{
				auto ambientItr = renderData.IDBegin(*newAmbientIDs);
				auto const& ambientItrEnd = renderData.IDEnd(*newAmbientIDs);
				while (ambientItr != ambientItrEnd)
				{
					gr::Light::RenderDataAmbientIBL const& ambientData = ambientItr.Get<gr::Light::RenderDataAmbientIBL>();

					const gr::RenderDataID lightID = ambientData.m_renderDataID;

					gr::MeshPrimitive::RenderData const& ambientMeshPrimData =
						ambientItr.Get<gr::MeshPrimitive::RenderData>();

					re::Texture const* iblTex = ambientData.m_iblTex;
					SEAssert(iblTex, "IBL texture cannot be null");

					std::shared_ptr<re::Texture> iemTex = nullptr;
					PopulateIEMTex(m_resourceCreationStagePipeline, iblTex, iemTex);

					std::shared_ptr<re::Texture> pmremTex = nullptr;
					PopulatePMREMTex(m_resourceCreationStagePipeline, iblTex, pmremTex);

					const uint32_t totalPMREMMipLevels = pmremTex->GetNumMips();

					const AmbientLightData ambientLightParamsData = GetAmbientLightParamsData(
						totalPMREMMipLevels,
						ambientData.m_diffuseScale,
						ambientData.m_specularScale,
						static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey)),
						m_ssaoTex.get());

					std::shared_ptr<re::Buffer> ambientParams = re::Buffer::Create(
						AmbientLightData::s_shaderName,
						ambientLightParamsData,
						re::Buffer::Type::Mutable);

					m_ambientLightData.emplace(ambientData.m_renderDataID,
						AmbientLightRenderData{
							.m_ambientParams = ambientParams,
							.m_IEMTex = iemTex,
							.m_PMREMTex = pmremTex,
							.m_batch = re::Batch(re::Batch::Lifetime::Permanent, ambientMeshPrimData, nullptr)
						});

					// Set the batch inputs:
					re::Batch& ambientBatch = m_ambientLightData.at(lightID).m_batch;

					ambientBatch.SetEffectID(effect::Effect::ComputeEffectID("DeferredLighting"));

					ambientBatch.AddTextureInput(
						"CubeMapIEM",
						iemTex,
						re::Sampler::GetSampler("WrapMinMagMipLinear"),
						re::TextureView(iemTex));

					ambientBatch.AddTextureInput(
						"CubeMapPMREM",
						pmremTex,
						re::Sampler::GetSampler("WrapMinMagMipLinear"),
						re::TextureView(pmremTex));

					ambientBatch.SetBuffer(ambientParams);

					++ambientItr;
				}
			}
		}

		// Update the params of the ambient lights we're tracking:
		for (auto const& ambientLight : m_ambientLightData)
		{
			const gr::RenderDataID lightID = ambientLight.first;

			if (renderData.IsDirty<gr::Light::RenderDataAmbientIBL>(lightID))
			{
				gr::Light::RenderDataAmbientIBL const& ambientRenderData =
					renderData.GetObjectData<gr::Light::RenderDataAmbientIBL>(lightID);

				const uint32_t totalPMREMMipLevels = ambientLight.second.m_PMREMTex->GetNumMips();

				const AmbientLightData ambientLightParamsData = GetAmbientLightParamsData(
					totalPMREMMipLevels,
					ambientRenderData.m_diffuseScale,
					ambientRenderData.m_specularScale,
					static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey)),
					m_ssaoTex.get());

				ambientLight.second.m_ambientParams->Commit(ambientLightParamsData);
			}
		}

		// Update the shared active ambient light pointers:
		if (m_graphicsSystemManager->HasActiveAmbientLight() && 
			m_graphicsSystemManager->GetActiveAmbientLightID() != m_activeAmbientLightData.m_renderDataID)
		{
			const gr::RenderDataID activeAmbientID = m_graphicsSystemManager->GetActiveAmbientLightID();

			SEAssert(m_ambientLightData.contains(activeAmbientID), "Cannot find active ambient light");

			AmbientLightRenderData& activeAmbientLightData = m_ambientLightData.at(activeAmbientID);

			m_activeAmbientLightData.m_renderDataID = activeAmbientID;
			m_activeAmbientLightData.m_ambientParams = activeAmbientLightData.m_ambientParams;
			m_activeAmbientLightData.m_IEMTex = activeAmbientLightData.m_IEMTex;
			m_activeAmbientLightData.m_PMREMTex = activeAmbientLightData.m_PMREMTex;
		}

		using ShadowTextureMap = std::unordered_map<gr::RenderDataID, re::Texture const*>;
		
		ShadowTextureMap const* shadowTextures = 
			static_cast<ShadowTextureMap const*>(dataDependencies.at(k_shadowTexturesInput));
		
		// Register new directional lights:
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataDirectional>())
		{
			std::vector<gr::RenderDataID> const* newDirectionalIDs =
				renderData.GetIDsWithNewData<gr::Light::RenderDataDirectional>();
			if (newDirectionalIDs)
			{
				auto directionalItr = renderData.IDBegin(*newDirectionalIDs);
				auto const& directionalItrEnd = renderData.IDEnd(*newDirectionalIDs);
				while (directionalItr != directionalItrEnd)
				{
					gr::Light::RenderDataDirectional const& directionalData =
						directionalItr.Get<gr::Light::RenderDataDirectional>();

					gr::Transform::RenderData const& directionalTransformData = directionalItr.GetTransformData();
					gr::MeshPrimitive::RenderData const& meshData = directionalItr.Get<gr::MeshPrimitive::RenderData>();

					gr::ShadowMap::RenderData const* shadowData = nullptr;
					gr::Camera::RenderData const* shadowCamData = nullptr;
					if (directionalData.m_hasShadow)
					{
						shadowData = &renderData.GetObjectData<gr::ShadowMap::RenderData>(directionalData.m_renderDataID);
						shadowCamData = &renderData.GetObjectData<gr::Camera::RenderData>(directionalData.m_renderDataID);
					}

					m_punctualLightData.emplace(
						directionalItr.GetRenderDataID(),
						PunctualLightRenderData{
							.m_type = gr::Light::Directional,
							.m_transformParams = nullptr,
							.m_batch = re::Batch(re::Batch::Lifetime::Permanent, meshData, nullptr)
						});

					re::Batch& directionalLightBatch = m_punctualLightData.at(directionalData.m_renderDataID).m_batch;

					directionalLightBatch.SetEffectID(effect::Effect::ComputeEffectID("DeferredLighting"));

					if (directionalData.m_hasShadow) // Add the shadow map texture to the batch
					{
						re::Texture const* shadowTex = shadowTextures ?
							shadowTextures->at(directionalData.m_renderDataID) : m_missing2DShadowFallback.get();

						directionalLightBatch.AddTextureInput(
							"Depth0",
							shadowTex,
							re::Sampler::GetSampler("BorderCmpMinMagLinearMipPoint").get(),
							re::TextureView(shadowTex));
					}

					++directionalItr;
				}
			}
		}


		auto RegisterNewDeferredMeshLight = [&](
			gr::RenderDataManager::IDIterator const& lightItr,
			gr::Light::Type lightType,
			void const* lightRenderData,
			bool hasShadow,
			std::unordered_map<gr::RenderDataID, PunctualLightRenderData>& punctualLightData,
			char const* depthInputTexName,
			char const* samplerTypeName)
			{
				gr::MeshPrimitive::RenderData const& meshData = lightItr.Get<gr::MeshPrimitive::RenderData>();
				gr::Transform::RenderData const& transformData = lightItr.GetTransformData();

				gr::ShadowMap::RenderData const* shadowData = nullptr;
				gr::Camera::RenderData const* shadowCamData = nullptr;
				if (hasShadow)
				{
					shadowData = &lightItr.Get<gr::ShadowMap::RenderData>();
					shadowCamData = &lightItr.Get<gr::Camera::RenderData>();
				}

				std::shared_ptr<re::Buffer> transformBuffer = gr::Transform::CreateInstancedTransformBuffer(
					re::Buffer::Type::Mutable, transformData);

				punctualLightData.emplace(
					lightItr.GetRenderDataID(),
					PunctualLightRenderData{
						.m_type = lightType,
						.m_transformParams = transformBuffer,
						.m_batch = re::Batch(re::Batch::Lifetime::Permanent, meshData, nullptr)
					});

				const gr::RenderDataID lightRenderDataID = lightItr.GetRenderDataID();

				re::Batch& lightBatch = punctualLightData.at(lightRenderDataID).m_batch;

				lightBatch.SetEffectID(effect::Effect::ComputeEffectID("DeferredLighting"));

				lightBatch.SetBuffer(transformBuffer);

				if (hasShadow) // Add the shadow map texture to the batch
				{
					re::Texture const* shadowTex = nullptr;
					if (shadowTextures)
					{
						shadowTex = shadowTextures->at(lightRenderDataID);
					}
					else
					{
						switch (lightType)
						{
						case gr::Light::Type::Point:
						{
							shadowTex = m_missingCubeShadowFallback.get();
						}
						break;
						case gr::Light::Type::Spot:
						{
							shadowTex = m_missing2DShadowFallback.get();
						}
						break;
						default: SEAssertF("Invalid/unexpected light type");
						}
					}

					lightBatch.AddTextureInput(
						depthInputTexName,
						shadowTex,
						re::Sampler::GetSampler(util::HashKey::Create(samplerTypeName)).get(),
						re::TextureView(shadowTex));
				}
			};
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataPoint>())
		{
			std::vector<gr::RenderDataID> const* newPointIDs = renderData.GetIDsWithNewData<gr::Light::RenderDataPoint>();
			if (newPointIDs)
			{
				auto pointItr = renderData.IDBegin(*newPointIDs);
				auto const& pointItrEnd = renderData.IDEnd(*newPointIDs);
				while (pointItr != pointItrEnd)
				{
					gr::Light::RenderDataPoint const& pointData = pointItr.Get<gr::Light::RenderDataPoint>();
					const bool hasShadow = pointData.m_hasShadow;

					RegisterNewDeferredMeshLight(
						pointItr,
						gr::Light::Point,
						&pointData,
						hasShadow,
						m_punctualLightData,
						"CubeDepth",
						"WrapCmpMinMagLinearMipPoint");

					++pointItr;
				}
			}
		}
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataSpot>())
		{
			std::vector<gr::RenderDataID> const* newSpotIDs = renderData.GetIDsWithNewData<gr::Light::RenderDataSpot>();

			if (newSpotIDs)
			{
				auto spotItr = renderData.IDBegin(*newSpotIDs);
				auto const& spotItrEnd = renderData.IDEnd(*newSpotIDs);
				while (spotItr != spotItrEnd)
				{
					gr::Light::RenderDataSpot const& spotData = spotItr.Get<gr::Light::RenderDataSpot>();
					const bool hasShadow = spotData.m_hasShadow;

					RegisterNewDeferredMeshLight(
						spotItr,
						gr::Light::Spot,
						&spotData,
						hasShadow,
						m_punctualLightData,
						"Depth0",
						"WrapCmpMinMagLinearMipPoint");

					++spotItr;
				}
			}
		}

		// Attach the single-frame monolithic light data buffers:
		gr::LightManager const& lightMgr = re::RenderManager::Get()->GetLightManager();
		
		m_directionalStage->AddSingleFrameBuffer(lightMgr.GetLightDataBuffer(gr::Light::Directional));
		m_pointStage->AddSingleFrameBuffer(lightMgr.GetLightDataBuffer(gr::Light::Point));
		m_spotStage->AddSingleFrameBuffer(lightMgr.GetLightDataBuffer(gr::Light::Spot));

		CreateBatches(dataDependencies);
	}


	void DeferredLightingGraphicsSystem::CreateBatches(DataDependencies const& dataDependencies)
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		if (m_activeAmbientLightData.m_renderDataID != gr::k_invalidRenderDataID)
		{
			SEAssert(m_ambientLightData.contains(m_activeAmbientLightData.m_renderDataID),
				"Cannot find active ambient light");

			m_ambientStage->AddBatch(m_ambientLightData.at(m_activeAmbientLightData.m_renderDataID).m_batch);
		}
		

		// Hash culled visible light IDs so we can quickly check if we need to add a point/spot light's batch:
		std::unordered_set<gr::RenderDataID> visibleLightIDs;

		auto MarkIDsVisible = [&](std::vector<gr::RenderDataID> const* lightIDs)
			{
				for (gr::RenderDataID lightID : *lightIDs)
				{
					visibleLightIDs.emplace(lightID);
				}
			};
		auto MarkAllIDsVisible = [&](auto& lightObjectItr, auto const& lightObjectItrEnd)
			{
				while (lightObjectItr != lightObjectItrEnd)
				{
					visibleLightIDs.emplace(lightObjectItr.GetRenderDataID());
					++lightObjectItr;
				}
			};

		PunctualLightCullingResults const* spotIDs = 
			static_cast<PunctualLightCullingResults const*>(dataDependencies.at(k_spotLightCullingDataInput));
		if (spotIDs)
		{
			MarkIDsVisible(spotIDs);
		}
		else
		{
			auto spotItr = renderData.ObjectBegin<gr::Light::RenderDataSpot>();
			auto const& spotItrEnd = renderData.ObjectEnd<gr::Light::RenderDataSpot>();
			MarkAllIDsVisible(spotItr, spotItrEnd);
		}

		PunctualLightCullingResults const* pointIDs = 
			static_cast<PunctualLightCullingResults const*>(dataDependencies.at(k_pointLightCullingDataInput));
		if (pointIDs)
		{
			MarkIDsVisible(pointIDs);
		}
		else
		{
			auto pointItr = renderData.ObjectBegin<gr::Light::RenderDataPoint>();
			auto const& pointItrEnd = renderData.ObjectEnd<gr::Light::RenderDataPoint>();
			MarkAllIDsVisible(pointItr, pointItrEnd);
		}


		gr::LightManager const& lightMgr = re::RenderManager::Get()->GetLightManager();


		// Update all of the punctual lights we're tracking:
		for (auto& light : m_punctualLightData)
		{
			const gr::RenderDataID lightID = light.first;

			// Update lighting buffers, if anything is dirty:
			const bool transformIsDirty = renderData.TransformIsDirtyFromRenderDataID(lightID);

			const bool lightRenderDataDirty = 
				(light.second.m_type == gr::Light::Type::Directional &&
					renderData.IsDirty<gr::Light::RenderDataDirectional>(lightID)) ||
				(light.second.m_type == gr::Light::Type::Point &&
					renderData.IsDirty<gr::Light::RenderDataPoint>(lightID)) ||
				(light.second.m_type == gr::Light::Type::Spot &&
					renderData.IsDirty<gr::Light::RenderDataSpot>(lightID));

			const bool shadowDataIsDirty = 
				(renderData.HasObjectData<gr::ShadowMap::RenderData>(lightID) && 
					renderData.IsDirty<gr::ShadowMap::RenderData>(lightID) ) ||
				(renderData.HasObjectData<gr::Camera::RenderData>(lightID) &&
					renderData.IsDirty<gr::Camera::RenderData>(lightID));

			if (transformIsDirty || lightRenderDataDirty || shadowDataIsDirty)
			{
				gr::Transform::RenderData const& lightTransformData =
					renderData.GetTransformDataFromRenderDataID(lightID);

				void const* lightRenderData = nullptr;
				gr::ShadowMap::RenderData const* shadowData = nullptr;
				gr::Camera::RenderData const* shadowCamData = nullptr;
				bool hasShadow = false;

				switch (light.second.m_type)
				{
				case gr::Light::Type::Directional:
				{
					gr::Light::RenderDataDirectional const& directionalData =
						renderData.GetObjectData<gr::Light::RenderDataDirectional>(lightID);
					light.second.m_canContribute = directionalData.m_canContribute;
					lightRenderData = &directionalData;
					hasShadow = directionalData.m_hasShadow;
				}
				break;
				case gr::Light::Type::Point:
				{
					gr::Light::RenderDataPoint const& pointData = 
						renderData.GetObjectData<gr::Light::RenderDataPoint>(lightID);
					light.second.m_canContribute = pointData.m_canContribute;
					lightRenderData = &pointData;
					hasShadow = pointData.m_hasShadow;

					light.second.m_transformParams->Commit(gr::Transform::CreateInstancedTransformData(
							renderData.GetTransformDataFromRenderDataID(lightID)));
				}
				break;
				case gr::Light::Type::Spot:
				{
					gr::Light::RenderDataSpot const& spotData =
						renderData.GetObjectData<gr::Light::RenderDataSpot>(lightID);
					light.second.m_canContribute = spotData.m_canContribute;
					lightRenderData = &spotData;
					hasShadow = spotData.m_hasShadow;

					light.second.m_transformParams->Commit(gr::Transform::CreateInstancedTransformData(
						renderData.GetTransformDataFromRenderDataID(lightID)));
				}
				break;
				default: SEAssertF("Invalid light type");
				}
			}


			// Add punctual batches:
			if (light.second.m_canContribute &&
				(light.second.m_type == gr::Light::Type::Directional || 
					visibleLightIDs.contains(lightID)))
			{
				auto AddBatchAndLightIndexDataBuffer = [&light, &lightID, &lightMgr](re::RenderStage* stage)
					{
						re::Batch* duplicatedBatch =
							stage->AddBatchWithLifetime(light.second.m_batch, re::Batch::Lifetime::SingleFrame);

						duplicatedBatch->SetBuffer(
							lightMgr.GetLightIndexDataBuffer(light.second.m_type, lightID, LightIndexData::s_shaderName));
					};

				switch (light.second.m_type)
				{
				case gr::Light::Type::Directional:
				{					
					AddBatchAndLightIndexDataBuffer(m_directionalStage.get());
				}
				break;
				case gr::Light::Type::Point:
				{
					AddBatchAndLightIndexDataBuffer(m_pointStage.get());
				}
				break;
				case gr::Light::Type::Spot:
				{
					AddBatchAndLightIndexDataBuffer(m_spotStage.get());
				}
				break;
				case gr::Light::Type::AmbientIBL:
				default: SEAssertF("Invalid light type");
				}
			}
		}
	}
}