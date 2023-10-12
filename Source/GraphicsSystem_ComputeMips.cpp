// © 2023 Adam Badke. All rights reserved.
#include "ConfigKeys.h"
#include "GraphicsSystem_ComputeMips.h"
#include "MathUtils.h"
#include "RenderManager.h"


namespace
{
	struct MipGenerationParams
	{
		glm::vec4 g_output0Dimensions; // .xyzw = width, height, 1/width, 1/height of the output0 texture
		glm::uvec4 g_mipParams; // .xyzw = srcMipLevel, numMips, srcDimensionMode, faceIdx
		bool g_isSRGB;
	};

	MipGenerationParams CreateMipGenerationParamsData(
		std::shared_ptr<re::Texture> tex, uint32_t srcMipLevel, uint32_t numMips, uint32_t faceIdx)
	{
		const uint32_t output0MipLevel = srcMipLevel + 1;
		const glm::vec4 output0Dimensions = tex->GetSubresourceDimensions(output0MipLevel);

		/* Calculate the odd/even flag:
		#define SRC_WIDTH_EVEN_HEIGHT_EVEN 0
		#define SRC_WIDTH_ODD_HEIGHT_EVEN 1
		#define SRC_WIDTH_EVEN_HEIGHT_ODD 2
		#define SRC_WIDTH_ODD_HEIGHT_ODD 3 */
		const glm::vec4 srcDimensions = tex->GetSubresourceDimensions(srcMipLevel);

		uint32_t srcDimensionMode = (static_cast<uint32_t>(srcDimensions.x) % 2); // 1 if x is odd
		srcDimensionMode |= ((static_cast<uint32_t>(srcDimensions.y) % 2) << 1); // |= (1 << 1) if y is odd (2 or 3)
		
		MipGenerationParams mipGenerationParams{};
		mipGenerationParams.g_output0Dimensions = output0Dimensions;
		mipGenerationParams.g_mipParams = glm::uvec4(srcMipLevel, numMips, srcDimensionMode, 0);
		mipGenerationParams.g_isSRGB = tex->IsSRGB();

		return mipGenerationParams;
	}
}


namespace gr
{
	constexpr char const* k_gsName = "Compute Mips Graphics System";


	ComputeMipsGraphicsSystem::ComputeMipsGraphicsSystem()
		: GraphicsSystem(k_gsName)
		, NamedObject(k_gsName)
		, m_mipMapGenerationShader(nullptr)
		, m_stagePipeline(nullptr)
	{
	}


	void ComputeMipsGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		m_mipMapGenerationShader = re::Shader::GetOrCreate(en::ShaderNames::k_generateMipMapsShaderName, re::PipelineState());

		re::RenderStage::ComputeStageParams parentStageParams; // Defaults

		m_stagePipeline = &pipeline;

		m_parentStageItr = m_stagePipeline->AppendRenderStage(
			re::RenderStage::CreateComputeStage("MIP Generation Parent stage", parentStageParams));
	}


	void ComputeMipsGraphicsSystem::PreRender()
	{
		std::vector<std::shared_ptr<re::Texture>> const& newTextures = 
			re::RenderManager::Get()->GetNewlyCreatedTextures();
		if (newTextures.empty())
		{
			return;
		}

		//CreateBatches();

		std::shared_ptr<re::Sampler> const mipSampler =
			re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Clamp_Linear_Linear);

		re::StagePipeline::StagePipelineItr insertItr = m_parentStageItr;

		for (std::shared_ptr<re::Texture> newTexture : newTextures)
		{
			re::Texture::TextureParams const& textureParams = newTexture->GetTextureParams();
			if (textureParams.m_mipMode != re::Texture::MipMode::AllocateGenerate)
			{
				continue;
			}

			const uint32_t totalMipLevels = newTexture->GetNumMips(); // Includes mip 0

			for (uint32_t faceIdx = 0; faceIdx < textureParams.m_faces; faceIdx++)
			{
				constexpr uint32_t k_maxTargetsPerStage = 4;
				uint32_t targetMip = 1;
				while (targetMip < totalMipLevels)
				{
					const uint32_t firstTargetMipIdx = targetMip;
					const uint32_t sourceMip = targetMip - 1;

					const uint32_t numMipStages =
						targetMip + k_maxTargetsPerStage < totalMipLevels ? k_maxTargetsPerStage : (totalMipLevels - targetMip);

					re::RenderStage::ComputeStageParams computeStageParams; // Defaults, for now...

					const std::string stageName = std::format("{}: Face {}/{}, MIP {}-{}", 
						newTexture->GetName().c_str(),
						faceIdx + 1,
						textureParams.m_faces,
						firstTargetMipIdx,
						firstTargetMipIdx + numMipStages - 1);

					std::shared_ptr<re::RenderStage> mipGenerationStage = re::RenderStage::CreateSingleFrameComputeStage(
						stageName,
						computeStageParams);

					mipGenerationStage->SetStageShader(m_mipMapGenerationShader);
					
					mipGenerationStage->AddTextureInput("SrcTex", newTexture, mipSampler, sourceMip);

					MipGenerationParams const& mipGenerationParams =
						CreateMipGenerationParamsData(newTexture, sourceMip, numMipStages, faceIdx);

					mipGenerationStage->AddSingleFrameParameterBlock(re::ParameterBlock::Create(
						"MipGenerationParams", 
						mipGenerationParams,
						re::ParameterBlock::PBType::SingleFrame));

					const std::string targetSetName = newTexture->GetName() + 
						std::format(" MIP {} - {} generation stage targets", targetMip, targetMip + numMipStages - 1);

					std::shared_ptr<re::TextureTargetSet> mipGenTargets = re::TextureTargetSet::Create(targetSetName);

					for (uint32_t currentTargetIdx = 0; currentTargetIdx < numMipStages; currentTargetIdx++)
					{
						re::TextureTarget::TargetParams mipTargetParams;
						mipTargetParams.m_targetFace = faceIdx;
						mipTargetParams.m_targetMip = targetMip++;

						mipGenTargets->SetColorTarget(currentTargetIdx, newTexture, mipTargetParams);
					}
					mipGenerationStage->SetTextureTargetSet(mipGenTargets);

					// We (currently) use 8x8 thread group dimensions
					constexpr uint32_t k_numThreadsX = 8;
					constexpr uint32_t k_numThreadsY = 8;
					
					// Non-integer MIP dimensions are rounded down to the nearest integer
					glm::vec2 subresourceDimensions = newTexture->GetSubresourceDimensions(firstTargetMipIdx).xy;
					subresourceDimensions = glm::floor(subresourceDimensions);

					const glm::uvec2 firstTargetMipDimensions = glm::uvec2(
						subresourceDimensions.x,
						subresourceDimensions.y);

					// We want to dispatch enough k_numThreadsX x k_numThreadsY threadgroups to cover every pixel in
					// our 1st mip level (each thread samples a 2x2 block in the source level above the 1st mip target)
					const uint32_t roundedXDim = std::max(util::RoundUpToNearestMultiple<uint32_t>(
						firstTargetMipDimensions.x / k_numThreadsX, k_numThreadsX), 1u);
					const uint32_t roundedYDim = std::max(util::RoundUpToNearestMultiple<uint32_t>(
						firstTargetMipDimensions.y / k_numThreadsY, k_numThreadsY), 1u);

					// Add our dispatch information to a compute batch:
					re::Batch computeBatch = re::Batch(re::Batch::ComputeParams{
						.m_threadGroupCount = glm::uvec3(roundedXDim, roundedYDim, 1u) });

					mipGenerationStage->AddBatch(computeBatch);

					insertItr = m_stagePipeline->AppendSingleFrameRenderStage(insertItr, std::move(mipGenerationStage));
				}
			}			
		}
	}


	std::shared_ptr<re::TextureTargetSet const> ComputeMipsGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return nullptr;
	}


	void ComputeMipsGraphicsSystem::CreateBatches()
	{
		// TODO: Is this required?
		// -> Perhaps we should hide this as a no-op behind a "ComputeGraphicsSystem" interface?
	}
}
