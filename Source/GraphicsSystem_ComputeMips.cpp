// © 2023 Adam Badke. All rights reserved.
#include "ConfigKeys.h"
#include "GraphicsSystem_ComputeMips.h"


namespace
{

	struct MipGenerationParams
	{
		glm::vec4 g_output0Dimensions; // .xyzw = width, height, 1/width, 1/height of the output0 texture
		glm::uvec4 g_mipParams; // .xyzw = srcMipLevel, numMips, srcDimensionMode, 0
		bool g_isSRGB;
	};

	MipGenerationParams CreateMipGenerationParamsData(
		std::shared_ptr<re::Texture> tex, uint32_t srcMipLevel, uint32_t numMips)
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
	ComputeMipsGraphicsSystem::ComputeMipsGraphicsSystem(std::string name)
		: GraphicsSystem(name)
		, NamedObject(name)
		, m_mipMapGenerationShader(nullptr)
	{
	}


	void ComputeMipsGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		m_mipMapGenerationShader = re::Shader::Create(en::ShaderNames::k_mipGenerationShaderName);
	}


	void ComputeMipsGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		if (m_textures.empty())
		{
			return;
		}

		//CreateBatches();

		std::shared_ptr<re::Sampler> const mipSampler =
			re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::ClampLinearLinear);

		for (std::shared_ptr<re::Texture> newTexture : m_textures)
		{
			re::Texture::TextureParams const& textureParams = newTexture->GetTextureParams();
			SEAssert("Trying to generate MIPs for a texture that does not use them", textureParams.m_useMIPs == true);

			const uint32_t totalMipLevels = newTexture->GetNumMips(); // Includes mip 0

			for (uint32_t faceIdx = 0; faceIdx < textureParams.m_faces; faceIdx++)
			{
				const uint32_t numTargetsPerStage = 4;
				uint32_t targetMip = 1;
				while (targetMip < totalMipLevels)
				{
					re::RenderStage::ComputeStageParams computeStageParams; // Defaults, for now...

					std::shared_ptr<re::RenderStage> mipGenerationStage = re::RenderStage::CreateComputeStage(
						newTexture->GetName() + " MIP generation stage",
						computeStageParams);

					mipGenerationStage->SetStageShader(m_mipMapGenerationShader);

					const uint32_t sourceMip = targetMip - 1;
					mipGenerationStage->AddTextureInput("SrcTex", newTexture, mipSampler, sourceMip);

					const uint32_t numMipStages =
						targetMip + numTargetsPerStage < totalMipLevels ? numTargetsPerStage : (totalMipLevels - targetMip);

					mipGenerationStage->AddSingleFrameParameterBlock(re::ParameterBlock::Create(
						"MipGenerationParams", 
						CreateMipGenerationParamsData(newTexture, sourceMip, numMipStages),
						re::ParameterBlock::PBType::SingleFrame));

					const std::string targetSetName = newTexture->GetName() + 
						std::format(" MIP {} - {} generation stage targets", targetMip, targetMip + numMipStages - 1);

					std::shared_ptr<re::TextureTargetSet> mipGenTargets = re::TextureTargetSet::Create(targetSetName);

					for (uint32_t currentTargetIdx = 0; currentTargetIdx < numMipStages; currentTargetIdx++)
					{
						re::TextureTarget::TargetParams mipTargetParams;
						mipTargetParams.m_targetFace = faceIdx;
						mipTargetParams.m_targetSubesource = targetMip++;

						mipGenTargets->SetColorTarget(currentTargetIdx, newTexture, mipTargetParams);
					}
					mipGenerationStage->SetTextureTargetSet(mipGenTargets);

					// We want to dispatch 1 thread per texel for our first downsampled mip level (as it will sample the
					// 2x2 block above it)
					const glm::vec2 firstDstMipDimensions = newTexture->GetSubresourceDimensions(sourceMip).xy * 0.5f;

					// Add our dispatch information to a compute batch:
					re::Batch computeBatch = re::Batch(re::Batch::ComputeParams{
						.m_threadGroupCount = glm::uvec3(
							firstDstMipDimensions.x,
							firstDstMipDimensions.y,
							1)});

					mipGenerationStage->AddBatch(computeBatch);

					pipeline.AppendSingleFrameRenderStage(std::move(mipGenerationStage));
				}
			}			
		}
		m_textures.clear();
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


	void ComputeMipsGraphicsSystem::AddTexture(std::shared_ptr<re::Texture> texture)
	{
		m_textures.emplace_back(texture);
	}
}
