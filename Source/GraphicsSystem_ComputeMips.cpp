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
				for (uint32_t currentMip = 1; currentMip < totalMips; currentMip++)
				{
					// TODO: We want to compute several levels of MIPs for a 8x8 block of texels in a single dispatch

					re::RenderStage::ComputeStageParams computeStageParams;

					std::shared_ptr<re::RenderStage> mipGenerationStage = re::RenderStage::CreateComputeStage(
						newTexture->GetName() + " MIP generation stage",
						computeStageParams);

					const std::string targetSetName = newTexture->GetName() + " MIP generation stage targets";
					std::shared_ptr<re::TextureTargetSet> mipGenTargets = re::TextureTargetSet::Create(targetSetName);

					re::TextureTarget::TargetParams targetParams;
					targetParams.m_targetFace = faceIdx;
					targetParams.m_targetSubesource = currentMip;

					mipGenTargets->SetColorTarget(0, newTexture, targetParams);

					mipGenerationStage->SetTextureTargetSet(mipGenTargets);

					mipGenerationStage->SetStageShader(m_mipMapGenerationShader);

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
