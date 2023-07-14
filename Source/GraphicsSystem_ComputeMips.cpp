// � 2023 Adam Badke. All rights reserved.
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
		m_mipMapGenerationShader = re::Shader::Create("GenerateMipMaps");
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
				// TODO: We want to compute several levels of MIPs for a block of texels in a single dispatch
				uint32_t targetMIP = 1;
				
				re::RenderStage mipGenerationStage(newTexture->GetName() + " MIP generation stage");

				const std::string targetSetName = newTexture->GetName() + " MIP generation stage targets";
				//std::shared_ptr<re::TextureTargetSet> mipGenTargets = re::TextureTargetSet::Create(targetSetName);

				re::TextureTarget::TargetParams targetParams;
				targetParams.m_targetFace = faceIdx;
				targetParams.m_targetMip = targetMIP;

				// TODO: Create multiple targets for the subresources we'll be writing to
				// -> Is it valid to have targets of different sizes?

				//mipGenerationStage.SetTextureTargetSet(mipGenTargets);
				

				mipGenerationStage.SetStageShader(m_mipMapGenerationShader);

				pipeline.AppendSingleFrameRenderStage(mipGenerationStage);
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
