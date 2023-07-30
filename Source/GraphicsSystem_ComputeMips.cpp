// © 2023 Adam Badke. All rights reserved.
#include "GraphicsSystem_ComputeMips.h"


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

					const uint32_t numStageTargets =
						currentMip + numTargetsPerStage < totalMips ? numTargetsPerStage : (totalMips - currentMip);

					const std::string targetSetName = newTexture->GetName() + 
						std::format(" MIP {} - {} generation stage targets", currentMip, currentMip + numStageTargets - 1);

					std::shared_ptr<re::TextureTargetSet> mipGenTargets = re::TextureTargetSet::Create(targetSetName);

					for (uint32_t currentTargetIdx = 0; currentTargetIdx < numStageTargets; currentTargetIdx++)
					{
						re::TextureTarget::TargetParams mipTargetParams;
						mipTargetParams.m_targetFace = faceIdx;
						mipTargetParams.m_targetSubesource = currentMip++;

						mipGenTargets->SetColorTarget(currentTargetIdx, newTexture, mipTargetParams);
					}

					mipGenerationStage->SetTextureTargetSet(mipGenTargets);

					// Add our dispatch information to a compute batch:
					re::Batch computeBatch = re::Batch(re::Batch::ComputeParams{
						.m_threadGroupCount = glm::uvec3(newTexture->Width(), newTexture->Height(), 1)
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
