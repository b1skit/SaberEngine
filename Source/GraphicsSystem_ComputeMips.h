// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


namespace gr
{
	class ComputeMipsGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		ComputeMipsGraphicsSystem(gr::GraphicsSystemManager*);

		~ComputeMipsGraphicsSystem() override {}

		void Create(re::StagePipeline&);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;


	private:
		void CreateBatches() override;


	private:
		re::StagePipeline::StagePipelineItr m_parentStageItr;
		re::StagePipeline* m_stagePipeline;

		std::shared_ptr<re::Shader> m_mipMapGenerationShader;
	};
}