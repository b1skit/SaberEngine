// © 2022 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "GraphicsSystemManager.h"
#include "GraphicsSystem_Bloom.h"
#include "Sampler.h"

#include "Core/Definitions/ConfigKeys.h"

#include "Shaders/Common/BloomComputeParams.h"


namespace
{
	constexpr char const* k_bloomTargetName = "output0";


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

		bloomComputeParams.g_srcTexDimensions = bloomSrcTex->GetMipLevelDimensions(srcMipLevel);
		bloomComputeParams.g_dstTexDimensions = bloomDstTex->GetMipLevelDimensions(dstMipLevel);
		
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
	BloomGraphicsSystem::BloomGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
	{
		m_firstUpsampleSrcMipLevel = 5; // == # of upsample stages
	}


	void BloomGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&, DataDependencies const&)
	{
		std::shared_ptr<re::Sampler> const bloomSampler = re::Sampler::GetSampler("ClampMinMagMipLinear");

		// Emissive blit:
		re::RenderStage::FullscreenQuadParams emissiveBlitParams{};
		emissiveBlitParams.m_effectID = effect::Effect::ComputeEffectID("Bloom");
		emissiveBlitParams.m_drawStyleBitmask = effect::drawstyle::Bloom_EmissiveBlit;

		m_emissiveBlitStage = re::RenderStage::CreateFullscreenQuadStage("Emissive blit stage", emissiveBlitParams);

		// Emissive blit texture inputs:
		m_emissiveBlitStage->AddPermanentTextureInput(
			"Tex0",
			*texDependencies.at(k_emissiveInput),
			bloomSampler,
			re::TextureView(*texDependencies.at(k_emissiveInput)));

		// Additively blit the emissive values to the deferred lighting target:
		std::shared_ptr<re::Texture> deferredLightTargetTex = *texDependencies.at(k_bloomTargetInput);

		std::shared_ptr<re::TextureTargetSet> emissiveTargetSet = 
			re::TextureTargetSet::Create("Emissive Blit Target Set");

		emissiveTargetSet->SetColorTarget(
			0,
			deferredLightTargetTex,
			re::TextureTarget::TargetParams{.m_textureView = re::TextureView::Texture2DView(0, 1)});

		emissiveTargetSet->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
			re::TextureTarget::BlendMode::One, re::TextureTarget::BlendMode::One });
		emissiveTargetSet->SetAllTargetClearModes(re::TextureTarget::ClearMode::Disabled);

		m_emissiveBlitStage->SetTextureTargetSet(emissiveTargetSet);

		// Append the emissive blit stage:
		pipeline.AppendRenderStage(m_emissiveBlitStage);


		// Bloom:
		
		// Bloom target: We create a single texture, and render into its mips
		const glm::uvec2 bloomTargetWidthHeight = 
			glm::uvec2(deferredLightTargetTex->Width() / 2, deferredLightTargetTex->Height() / 2);
		
		re::Texture::TextureParams bloomTargetTexParams;
		bloomTargetTexParams.m_width = bloomTargetWidthHeight.x;
		bloomTargetTexParams.m_height = bloomTargetWidthHeight.y;
		bloomTargetTexParams.m_usage = 
			static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::ColorSrc);
		bloomTargetTexParams.m_dimension = re::Texture::Dimension::Texture2D;
		bloomTargetTexParams.m_format = deferredLightTargetTex->GetTextureParams().m_format;
		bloomTargetTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		bloomTargetTexParams.m_mipMode = re::Texture::MipMode::Allocate;
		bloomTargetTexParams.m_addToSceneData = false;

		m_bloomTargetTex = re::Texture::Create("Bloom Target", bloomTargetTexParams);

		const uint32_t numBloomMips = m_bloomTargetTex->GetNumMips();

		// Downsample stages:
		for (uint32_t level = 0; level < numBloomMips; level++)
		{
			// Stage:
			std::string const& stageName = 
				std::format("Bloom downsample stage {}/{}: MIP {}", (level + 1), numBloomMips, level);
			std::shared_ptr<re::RenderStage> downStage = 
				re::RenderStage::CreateComputeStage(stageName.c_str(), re::RenderStage::ComputeStageParams());

			// Input:
			if (level == 0)
			{
				downStage->AddPermanentTextureInput(
					"Tex0", deferredLightTargetTex, bloomSampler, re::TextureView::Texture2DView(0, 1));
			}
			else
			{
				const uint32_t srcMipLvl = level - 1;

				downStage->AddPermanentTextureInput(
					"Tex0", m_bloomTargetTex, bloomSampler, re::TextureView::Texture2DView(srcMipLvl, 1));
			}

			// Target:
			downStage->AddPermanentRWTextureInput(
				k_bloomTargetName, m_bloomTargetTex, re::TextureView::Texture2DView(level, 1));

			// Buffers:
			std::shared_ptr<re::Buffer> bloomDownBuf = re::Buffer::Create(
				BloomComputeData::s_shaderName,
				BloomComputeData{}, // Populated during PreUpdate()
				re::Buffer::BufferParams{
					.m_stagingPool = re::Buffer::StagingPool::Permanent,
					.m_memPoolPreference = re::Buffer::UploadHeap,
					.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
					.m_usageMask = re::Buffer::Constant,
				});
			m_bloomDownBuffers.emplace_back(bloomDownBuf);
			downStage->AddPermanentBuffer(BloomComputeData::s_shaderName, bloomDownBuf);

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
			std::string const& stageName = 
				std::format("Bloom upsample stage {}/{}: MIP {}", upsampleNameLevel++, numUpsampleStages, upsampleDstMip);
			std::shared_ptr<re::RenderStage> upStage =
				re::RenderStage::CreateComputeStage(stageName.c_str(), re::RenderStage::ComputeStageParams());

			// Input:
			upStage->AddPermanentTextureInput(
				"Tex0", m_bloomTargetTex, bloomSampler, re::TextureView::Texture2DView(upsampleSrcMip, 1));

			// Targets:
			upStage->AddPermanentRWTextureInput(
				k_bloomTargetName, m_bloomTargetTex, re::TextureView::Texture2DView(upsampleDstMip, 1));

			// Buffers:
			std::shared_ptr<re::Buffer> bloomUpBuf = re::Buffer::Create(
				BloomComputeData::s_shaderName,
				BloomComputeData{}, // Populated during PreUpdate()
				re::Buffer::BufferParams{
					.m_stagingPool = re::Buffer::StagingPool::Permanent,
					.m_memPoolPreference = re::Buffer::UploadHeap,
					.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
					.m_usageMask = re::Buffer::Constant,
				});
			upStage->AddPermanentBuffer(BloomComputeData::s_shaderName, bloomUpBuf);
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
			&m_bloomTargetTex);
	}


	void BloomGraphicsSystem::PreRender()
	{
		CreateBatches();
	
		std::shared_ptr<re::Texture> deferredLightTargetTex = 
			m_emissiveBlitStage->GetTextureTargetSet()->GetColorTarget(0).GetTexture();

		gr::Camera::Config const& cameraConfig =
			m_graphicsSystemManager->GetActiveCameraRenderData().m_cameraConfig;

		// Buffers:
		const uint32_t numBloomMips = m_bloomTargetTex->GetNumMips();
		for (uint32_t level = 0; level < numBloomMips; level++)
		{
			BloomComputeData bloomComputeParams{};

			if (level == 0)
			{
				const uint32_t srcMipLevel = 0; // First mip of lighting target
				const uint32_t dstMipLevel = 0; // First mip of bloom target

				bloomComputeParams = CreateBloomComputeParamsData(
					deferredLightTargetTex,
					m_bloomTargetTex,
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
					m_bloomTargetTex,
					m_bloomTargetTex,
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
				m_bloomTargetTex,
				m_bloomTargetTex,
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
		const uint32_t numBloomTexMips = m_bloomTargetTex->GetNumMips();

		uint32_t downsampleDstMipLevel = 0;
		for (std::shared_ptr<re::RenderStage> downStage : m_bloomDownStages)
		{
			glm::vec2 dstMipWidthHeight = m_bloomTargetTex->GetMipLevelDimensions(downsampleDstMipLevel++).xy;

			re::Batch computeBatch = re::Batch(
				re::Lifetime::SingleFrame,
				re::Batch::ComputeParams{
					.m_threadGroupCount = glm::uvec3(dstMipWidthHeight.x, dstMipWidthHeight.y, 1u) },
				effect::Effect::ComputeEffectID("Bloom"));

			downStage->AddBatch(computeBatch);
		}

		uint32_t upsampleDstMipLevel = m_firstUpsampleSrcMipLevel - 1;
		for (std::shared_ptr<re::RenderStage> upStage : m_bloomUpStages)
		{
			glm::vec2 dstMipWidthHeight = m_bloomTargetTex->GetMipLevelDimensions(upsampleDstMipLevel--).xy;

			re::Batch computeBatch = re::Batch(
				re::Lifetime::SingleFrame,
				re::Batch::ComputeParams{
					.m_threadGroupCount = glm::uvec3(dstMipWidthHeight.x, dstMipWidthHeight.y, 1u) },
				effect::Effect::ComputeEffectID("Bloom"));

			upStage->AddBatch(computeBatch);
		}
	}
}