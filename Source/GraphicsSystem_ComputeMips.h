// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


namespace gr
{
	class ComputeMipsGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		explicit ComputeMipsGraphicsSystem(std::string name);

		~ComputeMipsGraphicsSystem() override {}

		void Create(re::StagePipeline&);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;

		// Add newly created textures; Mips will be generated in the next frame via a single-frame render stage
		void AddTexture(std::shared_ptr<re::Texture>);


	private:
		void CreateBatches() override;


	private:
		re::StagePipeline::StagePipelineItr m_parentStageItr;
		re::StagePipeline* m_stagePipeline;

		std::shared_ptr<re::Shader> m_mipMapGenerationShader;
		std::vector<std::shared_ptr<re::Texture>> m_textures;
	};
}