// © 2022 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "ConfigKeys.h"
#include "GraphicsSystemManager.h"
#include "GraphicsSystem_Bloom.h"
#include "Sampler.h"
#include "Shader.h"

#include "Shaders/Common/BloomComputeParams.h"


namespace
{
	BloomComputeData CreateBloomComputeParamsData(
		std::shared_ptr<re::Texture> bloomSrcTex,
		std::shared_ptr<re::Texture> bloomDstTex,
		uint32_t srcMipLevel,
		uint32_t dstMipLevel,
		bool isDownStage,
		uint32_t currentLevel,
		uint32_t numLevels,
		uint32_t firstUpsampleSrcMipLevel,
		gr::Camera::Config const& cameraConfig)
	{
		BloomComputeData bloomComputeParams{};

		bloomComputeParams.g_srcTexDimensions = bloomSrcTex->GetSubresourceDimensions(srcMipLevel);
		bloomComputeParams.g_dstTexDimensions = bloomDstTex->GetSubresourceDimensions(dstMipLevel);
		
		bloomComputeParams.g_srcMipDstMipFirstUpsampleSrcMipIsDownStage = glm::vec4(
			srcMipLevel,
			dstMipLevel,
			firstUpsampleSrcMipLevel,
			isDownStage);

		bloomComputeParams.g_bloomRadiusWidthHeightLevelNumLevls = glm::vec4(
			cameraConfig.m_bloomRadius.x,
			cameraConfig.m_bloomRadius.y,
			currentLevel,
			numLevels);
		
		bloomComputeParams.g_bloomDebug = glm::vec4(cameraConfig.m_deflickerEnabled, 0.f, 0.f, 0.f);

		return bloomComputeParams;
	}
}

namespace gr
{
	constexpr char const* k_gsName = "Bloom Graphics System";


	BloomGraphicsSystem::BloomGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(k_gsName, owningGSM)
		, NamedObject(k_gsName)
	{
		m_firstUpsampleSrcMipLevel = 5; // == # of upsample stages
	}


	void BloomGraphicsSystem::InitPipeline(re::StagePipeline& pipeline, TextureDependencies const& texDependencies)
	{
		std::shared_ptr<re::Sampler> const bloomSampler = re::Sampler::GetSampler("ClampMinMagMipLinear");

		// Emissive blit:
		m_emissiveBlitStage = 
			re::RenderStage::CreateFullscreenQuadStage("Emissive blit stage", re::RenderStage::FullscreenQuadParams{});

		// Blit shader:
		re::PipelineState blitPipelineState;
		blitPipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back);
		blitPipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Always);
		
		m_emissiveBlitStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_blitShaderName, blitPipelineState));

		// Emissive blit texture inputs:
		m_emissiveBlitStage->AddTextureInput(
			"Tex0",
			texDependencies.at(k_emissiveInput),
			bloomSampler);

		// Additively blit the emissive values to the deferred lighting target:
		std::shared_ptr<re::Texture> deferredLightTargetTex = texDependencies.at(k_bloomTargetInput);

		std::shared_ptr<re::TextureTargetSet> emissiveTargetSet = 
			re::TextureTargetSet::Create("Emissive Blit Target Set");

		emissiveTargetSet->SetColorTarget(0, deferredLightTargetTex, re::TextureTarget::TargetParams{});

		emissiveTargetSet->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
			re::TextureTarget::TargetParams::BlendMode::One, re::TextureTarget::TargetParams::BlendMode::One });
		emissiveTargetSet->SetAllTargetClearModes(re::TextureTarget::TargetParams::ClearMode::Disabled);

		m_emissiveBlitStage->SetTextureTargetSet(emissiveTargetSet);

		// Append the emissive blit stage:
		pipeline.AppendRenderStage(m_emissiveBlitStage);


		// Bloom:
		re::PipelineState bloomComputePipelineState; // Defaults
		m_bloomComputeShader = re::Shader::GetOrCreate(en::ShaderNames::k_bloomShaderName, bloomComputePipelineState);

		// Bloom target: We create a single texture, and render into its mips
		const glm::uvec2 bloomTargetWidthHeight = 
			glm::uvec2(deferredLightTargetTex->Width() / 2, deferredLightTargetTex->Height() / 2);
		
		re::Texture::TextureParams bloomTargetTexParams;
		bloomTargetTexParams.m_width = bloomTargetWidthHeight.x;
		bloomTargetTexParams.m_height = bloomTargetWidthHeight.y;
		bloomTargetTexParams.m_usage = 
			static_cast<re::Texture::Usage>(re::Texture::Usage::ComputeTarget | re::Texture::Usage::Color);
		bloomTargetTexParams.m_dimension = re::Texture::Dimension::Texture2D;
		bloomTargetTexParams.m_format = deferredLightTargetTex->GetTextureParams().m_format;
		bloomTargetTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		bloomTargetTexParams.m_mipMode = re::Texture::MipMode::Allocate;
		bloomTargetTexParams.m_addToSceneData = false;

		std::shared_ptr<re::Texture> bloomTargetTex = re::Texture::Create("Bloom Target", bloomTargetTexParams);

		const uint32_t numBloomMips = bloomTargetTex->GetNumMips();

		// Downsample stages:
		for (uint32_t level = 0; level < numBloomMips; level++)
		{
			// Stage:
			std::string const& stageName = 
				std::format("Bloom downsample stage {}/{}: MIP {}", (level + 1), numBloomMips, level);
			std::shared_ptr<re::RenderStage> downStage = 
				re::RenderStage::CreateComputeStage(stageName.c_str(), re::RenderStage::ComputeStageParams());

			// Shader:
			downStage->SetStageShader(m_bloomComputeShader);

			const std::string targetName = std::format("Bloom {}/{} Target Set", (level + 1), numBloomMips);
			std::shared_ptr<re::TextureTargetSet> bloomLevelTargets = re::TextureTargetSet::Create(targetName.c_str());

			glm::vec4 const& targetMipDimensions = bloomTargetTex->GetSubresourceDimensions(level);
			bloomLevelTargets->SetViewport(re::Viewport(
				0, 0, static_cast<uint32_t>(targetMipDimensions.x), static_cast<uint32_t>(targetMipDimensions.y)));
			bloomLevelTargets->SetScissorRect(re::ScissorRect(
				0, 0, static_cast<long>(targetMipDimensions.x), static_cast<long>(targetMipDimensions.y)));

			// Input:
			if (level == 0)
			{
				downStage->AddTextureInput("Tex0", deferredLightTargetTex, bloomSampler);
			}
			else
			{
				const uint32_t srcMipLevel = level - 1;

				downStage->AddTextureInput("Tex0", bloomTargetTex, bloomSampler, srcMipLevel);	
			}
			
			// Target:
			re::TextureTarget::TargetParams bloomLevelTargetParams;
			bloomLevelTargetParams.m_targetMip = level;
			
			bloomLevelTargets->SetColorTarget(
				0, 
				bloomTargetTex,
				bloomLevelTargetParams);

			downStage->SetTextureTargetSet(bloomLevelTargets);

			// Buffers:
			std::shared_ptr<re::Buffer> bloomDownBuf = re::Buffer::Create(
				BloomComputeData::s_shaderName,
				BloomComputeData{},
				re::Buffer::Type::Mutable);
			m_bloomDownBuffers.emplace_back(bloomDownBuf);
			downStage->AddPermanentBuffer(bloomDownBuf);

			pipeline.AppendRenderStage(downStage);

			m_bloomDownStages.emplace_back(downStage);
		}

		// Upsample stages:
		const uint32_t numUpsampleStages = m_firstUpsampleSrcMipLevel;
		uint32_t upsampleSrcMip = m_firstUpsampleSrcMipLevel;
		uint32_t upsampleNameLevel = 1;

		for (uint32_t level = numUpsampleStages; level >= 1; level--)
		{
			const uint32_t upsampleDstMip = upsampleSrcMip - 1;

			// Stage:
			const std::string stageName = 
				std::format("Bloom upsample stage {}/{}: MIP {}", upsampleNameLevel++, numUpsampleStages, upsampleDstMip);
			std::shared_ptr<re::RenderStage> upStage =
				re::RenderStage::CreateComputeStage(stageName.c_str(), re::RenderStage::ComputeStageParams());

			// Shader:
			upStage->SetStageShader(m_bloomComputeShader);

			const std::string targetName = std::format("Bloom {}/{} Target Set", (level + 1), numBloomMips);
			std::shared_ptr<re::TextureTargetSet> bloomLevelTargets = re::TextureTargetSet::Create(targetName.c_str());

			glm::vec4 const& targetMipDimensions = bloomTargetTex->GetSubresourceDimensions(upsampleDstMip);
			bloomLevelTargets->SetViewport(re::Viewport(
				0, 0, static_cast<uint32_t>(targetMipDimensions.x), static_cast<uint32_t>(targetMipDimensions.y)));
			bloomLevelTargets->SetScissorRect(re::ScissorRect(
				0, 0, static_cast<long>(targetMipDimensions.x), static_cast<long>(targetMipDimensions.y)));

			// Input:
			upStage->AddTextureInput("Tex0", bloomTargetTex, bloomSampler, upsampleSrcMip);

			// Targets:
			re::TextureTarget::TargetParams bloomLevelTargetParams;
			bloomLevelTargetParams.m_targetMip = upsampleDstMip;

			bloomLevelTargets->SetColorTarget(
				0,
				bloomTargetTex,
				bloomLevelTargetParams);

			upStage->SetTextureTargetSet(bloomLevelTargets);

			// Buffers:
			std::shared_ptr<re::Buffer> bloomUpBuf = re::Buffer::Create(
				BloomComputeData::s_shaderName,
				BloomComputeData{},
				re::Buffer::Type::Mutable);
			upStage->AddPermanentBuffer(bloomUpBuf);
			m_bloomUpBuffers.emplace_back(bloomUpBuf);

			pipeline.AppendRenderStage(upStage);

			m_bloomUpStages.emplace_back(upStage);

			upsampleSrcMip--;
		}
	}


	void BloomGraphicsSystem::RegisterInputs()
	{
		RegisterTextureInput(k_emissiveInput);
		RegisterTextureInput(k_bloomTargetInput);
	}


	void BloomGraphicsSystem::RegisterOutputs()
	{
		RegisterTextureOutput(
			k_bloomResultOutput, 
			m_bloomUpStages.back()->GetTextureTargetSet()->GetColorTarget(0).GetTexture());
	}


	void BloomGraphicsSystem::PreRender(DataDependencies const&)
	{
		CreateBatches();
	
		std::shared_ptr<re::Texture> deferredLightTargetTex = 
			m_emissiveBlitStage->GetTextureTargetSet()->GetColorTarget(0).GetTexture();
		
		std::shared_ptr<re::Texture> bloomTargetTex = 
			m_bloomUpStages.back()->GetTextureTargetSet()->GetColorTarget(0).GetTexture();

		gr::Camera::Config const& cameraConfig =
			m_graphicsSystemManager->GetActiveCameraRenderData().m_cameraConfig;

		// Buffers:
		const uint32_t numBloomMips = bloomTargetTex->GetNumMips();
		for (uint32_t level = 0; level < numBloomMips; level++)
		{
			BloomComputeData bloomComputeParams{};

			if (level == 0)
			{
				const uint32_t srcMipLevel = 0; // First mip of lighting target
				const uint32_t dstMipLevel = 0; // First mip of bloom target

				bloomComputeParams = CreateBloomComputeParamsData(
					deferredLightTargetTex,
					bloomTargetTex,
					srcMipLevel,
					dstMipLevel,
					true,
					level,
					numBloomMips,
					m_firstUpsampleSrcMipLevel,
					cameraConfig);
			}
			else
			{
				const uint32_t srcMipLevel = level - 1;
				const uint32_t dstMipLevel = srcMipLevel + 1;

				bloomComputeParams = CreateBloomComputeParamsData(
					bloomTargetTex,
					bloomTargetTex,
					srcMipLevel,
					dstMipLevel,
					true,
					level,
					numBloomMips,
					m_firstUpsampleSrcMipLevel, 
					cameraConfig);
			}

			m_bloomDownBuffers[level]->Commit(bloomComputeParams);
		}


		const uint32_t numUpsampleStages = m_firstUpsampleSrcMipLevel;
		uint32_t level = m_firstUpsampleSrcMipLevel;
		for (std::shared_ptr<re::Buffer> bloomUpBuf : m_bloomUpBuffers)
		{
			const uint32_t upsampleSrcMip = level;
			const uint32_t upsampleDstMip = upsampleSrcMip - 1;
			
			BloomComputeData const& bloomComputeParams = CreateBloomComputeParamsData(
				bloomTargetTex,
				bloomTargetTex,
				upsampleSrcMip,
				upsampleDstMip,
				false,
				level,
				numUpsampleStages,
				m_firstUpsampleSrcMipLevel,
				cameraConfig);

			bloomUpBuf->Commit(bloomComputeParams);

			level--;
		}
	}


	void BloomGraphicsSystem::CreateBatches()
	{
		std::shared_ptr<re::Texture> bloomTex = m_bloomDownStages[0]->GetTextureTargetSet()->GetColorTarget(0).GetTexture();
		const uint32_t numBloomTexMips = bloomTex->GetNumMips();

		uint32_t downsampleDstMipLevel = 0;
		for (std::shared_ptr<re::RenderStage> downStage : m_bloomDownStages)
		{
			glm::vec2 dstMipWidthHeight = bloomTex->GetSubresourceDimensions(downsampleDstMipLevel++).xy;

			re::Batch computeBatch = re::Batch(re::Batch::Lifetime::SingleFrame, re::Batch::ComputeParams{
						.m_threadGroupCount = glm::uvec3(dstMipWidthHeight.x, dstMipWidthHeight.y, 1u) });

			downStage->AddBatch(computeBatch);
		}

		uint32_t upsampleDstMipLevel = m_firstUpsampleSrcMipLevel - 1;
		for (std::shared_ptr<re::RenderStage> upStage : m_bloomUpStages)
		{
			glm::vec2 dstMipWidthHeight = bloomTex->GetSubresourceDimensions(upsampleDstMipLevel--).xy;

			re::Batch computeBatch = re::Batch(re::Batch::Lifetime::SingleFrame, re::Batch::ComputeParams{
						.m_threadGroupCount = glm::uvec3(dstMipWidthHeight.x, dstMipWidthHeight.y, 1u) });

			upStage->AddBatch(computeBatch);
		}
	}
}