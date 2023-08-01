// © 2023 Adam Badke. All rights reserved.
#include "GraphicsSystem_ComputeMips.h"


namespace
{
	struct MipGenerationParams
	{
		glm::vec4 g_textureDimensions; // .xyzw = width, height, 1/width, 1/height
		glm::uvec4 g_mipParams;
		bool g_isSRGB;
	};

	MipGenerationParams CreateMipGenerationParamsData(
		std::shared_ptr<re::Texture> tex, uint32_t srcMipLevel, uint32_t numMips)
	{
		MipGenerationParams mipGenerationParams{};
		mipGenerationParams.g_textureDimensions = tex->GetTextureDimenions();
		mipGenerationParams.g_mipParams = glm::uvec4(srcMipLevel, numMips, 0, 0);
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
		m_mipMapGenerationShader = re::Shader::Create("GenerateMipMaps_BoxFilter");
	}


	void ComputeMipsGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		//CreateBatches();

		for (std::shared_ptr<re::Texture> newTexture : m_textures)
		{
			re::Texture::TextureParams const& textureParams = newTexture->GetTextureParams();
			SEAssert("Trying to generate MIPs for a texture that does not use them", textureParams.m_useMIPs == true);

			const uint32_t totalMips = newTexture->GetNumMips(); // Includes mip 0

			for (uint32_t faceIdx = 0; faceIdx < textureParams.m_faces; faceIdx++)
			{
				const uint32_t numTargetsPerStage = 4;
				uint32_t currentMip = 1;
				while (currentMip < totalMips)
				{
					re::RenderStage::ComputeStageParams computeStageParams;

					std::shared_ptr<re::RenderStage> mipGenerationStage = re::RenderStage::CreateComputeStage(
						newTexture->GetName() + " MIP generation stage",
						computeStageParams);

					mipGenerationStage->SetStageShader(m_mipMapGenerationShader);

					std::shared_ptr<re::Sampler> const mipSampler =
						re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::ClampLinearLinear);

					mipGenerationStage->SetPerFrameTextureInput("SrcTex", newTexture, mipSampler, 0);

					const uint32_t numMipStages =
						currentMip + numTargetsPerStage < totalMips ? numTargetsPerStage : (totalMips - currentMip);

					mipGenerationStage->AddSingleFrameParameterBlock(re::ParameterBlock::Create(
						"MipGenerationParams", 
						CreateMipGenerationParamsData(newTexture, currentMip, numMipStages),
						re::ParameterBlock::PBType::SingleFrame));

					const std::string targetSetName = newTexture->GetName() + 
						std::format(" MIP {} - {} generation stage targets", currentMip, currentMip + numMipStages - 1);

					std::shared_ptr<re::TextureTargetSet> mipGenTargets = re::TextureTargetSet::Create(targetSetName);

					for (uint32_t currentTargetIdx = 0; currentTargetIdx < numMipStages; currentTargetIdx++)
					{
						re::TextureTarget::TargetParams mipTargetParams;
						mipTargetParams.m_targetFace = faceIdx;
						mipTargetParams.m_targetSubesource = currentMip++;

						mipGenTargets->SetColorTarget(currentTargetIdx, newTexture, mipTargetParams);
					}
					mipGenerationStage->SetTextureTargetSet(mipGenTargets);

					constexpr uint32_t k_computeBlockSize = 2; // Handle 2x2 blocks (skipping mip 0)

					// Add our dispatch information to a compute batch:
					re::Batch computeBatch = re::Batch(re::Batch::ComputeParams{
						.m_threadGroupCount = glm::uvec3(
							newTexture->Width() / k_computeBlockSize, 
							newTexture->Height() / k_computeBlockSize, 
							1)
						});
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
