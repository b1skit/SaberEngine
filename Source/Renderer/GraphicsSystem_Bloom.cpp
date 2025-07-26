// © 2022 Adam Badke. All rights reserved.
#include "BatchBuilder.h"
#include "CameraRenderData.h"
#include "Effect.h"
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystemCommon.h"
#include "GraphicsSystemManager.h"
#include "GraphicsUtils.h"
#include "RenderDataManager.h"
#include "RenderPipeline.h"
#include "Sampler.h"
#include "TextureView.h"

#include "Core/InvPtr.h"

#include "Renderer/Shaders/Common/BloomComputeParams.h"


namespace
{
	constexpr char const* k_bloomTargetName = "output0";

	static const EffectID k_bloomEffectID = effect::Effect::ComputeEffectID("Bloom");


	BloomComputeData CreateBloomComputeParamsData(
		core::InvPtr<re::Texture> bloomSrcTex,
		core::InvPtr<re::Texture> bloomDstTex,
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
		gr::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&, DataDependencies const&)
	{
		core::InvPtr<re::Sampler> const& bloomSampler = m_graphicsSystemManager->GetSampler("ClampMinMagMipLinear");

		// Emissive blit:
		gr::Stage::FullscreenQuadParams emissiveBlitParams{};
		emissiveBlitParams.m_effectID = k_bloomEffectID;
		emissiveBlitParams.m_drawStyleBitmask = effect::drawstyle::Bloom_EmissiveBlit;

		m_emissiveBlitStage = gr::Stage::CreateFullscreenQuadStage("Emissive blit stage", emissiveBlitParams);

		// Emissive blit texture inputs:
		core::InvPtr<re::Texture> const& emissiveTex = 
			*GetDependency<core::InvPtr<re::Texture>>(k_emissiveInput, texDependencies);
		m_emissiveBlitStage->AddPermanentTextureInput(
			"Tex0",
			emissiveTex,
			bloomSampler,
			re::TextureView(emissiveTex));

		// Additively blit the emissive values to the deferred lighting target:
		core::InvPtr<re::Texture> const& deferredLightTargetTex =
			*GetDependency<core::InvPtr<re::Texture>>(k_bloomTargetInput, texDependencies);

		std::shared_ptr<re::TextureTargetSet> emissiveTargetSet = 
			re::TextureTargetSet::Create("Emissive Blit Target Set");

		emissiveTargetSet->SetColorTarget(
			0,
			deferredLightTargetTex,
			re::TextureTarget::TargetParams{.m_textureView = re::TextureView::Texture2DView(0, 1)});

		m_emissiveBlitStage->SetTextureTargetSet(emissiveTargetSet);

		// Append the emissive blit stage:
		pipeline.AppendStage(m_emissiveBlitStage);


		// Bloom:
		
		// Bloom target: We create a single texture, and render into its mips
		const glm::uvec2 bloomTargetWidthHeight = 
			glm::uvec2(deferredLightTargetTex->Width() / 2, deferredLightTargetTex->Height() / 2);
		
		re::Texture::TextureParams bloomTargetTexParams;
		bloomTargetTexParams.m_width = bloomTargetWidthHeight.x;
		bloomTargetTexParams.m_height = bloomTargetWidthHeight.y;
		bloomTargetTexParams.m_usage = re::Texture::Usage::ColorTarget | re::Texture::Usage::ColorSrc;
		bloomTargetTexParams.m_dimension = re::Texture::Dimension::Texture2D;
		bloomTargetTexParams.m_format = deferredLightTargetTex->GetTextureParams().m_format;
		bloomTargetTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		bloomTargetTexParams.m_mipMode = re::Texture::MipMode::Allocate;
		bloomTargetTexParams.m_createAsPermanent = false;

		m_bloomTargetTex = re::Texture::Create("Bloom Target", bloomTargetTexParams);

		const uint32_t numBloomMips = m_bloomTargetTex->GetNumMips();

		// Downsample stages:
		for (uint32_t level = 0; level < numBloomMips; level++)
		{
			// Stage:
			std::string const& stageName = 
				std::format("Bloom downsample stage {}/{}: MIP {}", (level + 1), numBloomMips, level);
			std::shared_ptr<gr::Stage> downStage = 
				gr::Stage::CreateComputeStage(stageName.c_str(), gr::Stage::ComputeStageParams());

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

			pipeline.AppendStage(downStage);

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
			std::shared_ptr<gr::Stage> upStage =
				gr::Stage::CreateComputeStage(stageName.c_str(), gr::Stage::ComputeStageParams());

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

			pipeline.AppendStage(upStage);

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
		const gr::RenderDataID activeCamID = m_graphicsSystemManager->GetActiveCameraRenderDataID();
		if (activeCamID == gr::k_invalidRenderDataID)
		{
			return;
		}

		CreateBatches();
	
		core::InvPtr<re::Texture> deferredLightTargetTex = 
			m_emissiveBlitStage->GetTextureTargetSet()->GetColorTarget(0).GetTexture();

		gr::Camera::Config const& cameraConfig =
			m_graphicsSystemManager->GetRenderData().GetObjectData<gr::Camera::RenderData>(activeCamID).m_cameraConfig;


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
		for (std::shared_ptr<gr::Stage> downStage : m_bloomDownStages)
		{
			glm::vec2 const& dstMipWidthHeight = m_bloomTargetTex->GetMipLevelDimensions(downsampleDstMipLevel++).xy;
			
			gr::BatchHandle computeBatch = gr::ComputeBatchBuilder()
				.SetThreadGroupCount(glm::uvec3(
					grutil::GetRoundedDispatchDimension(static_cast<uint32_t>(dstMipWidthHeight.x), BLOOM_DISPATCH_XY_DIMS),
					grutil::GetRoundedDispatchDimension(static_cast<uint32_t>(dstMipWidthHeight.y), BLOOM_DISPATCH_XY_DIMS),
					1u))
				.SetEffectID(k_bloomEffectID)
				.Build();

			downStage->AddBatch(computeBatch);
		}

		uint32_t upsampleDstMipLevel = m_firstUpsampleSrcMipLevel - 1;
		for (std::shared_ptr<gr::Stage> upStage : m_bloomUpStages)
		{
			glm::vec2 const& dstMipWidthHeight = m_bloomTargetTex->GetMipLevelDimensions(upsampleDstMipLevel--).xy;
			
			gr::BatchHandle computeBatch = gr::ComputeBatchBuilder()
				.SetThreadGroupCount(glm::uvec3(
					grutil::GetRoundedDispatchDimension(static_cast<uint32_t>(dstMipWidthHeight.x), BLOOM_DISPATCH_XY_DIMS),
					grutil::GetRoundedDispatchDimension(static_cast<uint32_t>(dstMipWidthHeight.y), BLOOM_DISPATCH_XY_DIMS),
					1u))
				.SetEffectID(k_bloomEffectID)
				.Build();

			upStage->AddBatch(computeBatch);
		}
	}
}