#pragma once

#include <vector>
#include <string>

#include "GraphicsSystem.h"


namespace gr
{
	class ShadowsGraphicsSystem : public virtual GraphicsSystem
	{
	public:
		explicit ShadowsGraphicsSystem(std::string name);

		ShadowsGraphicsSystem() = delete;
		~ShadowsGraphicsSystem() override {}

		void Create(re::StagePipeline& pipeline) override;

		void PreRender(re::StagePipeline& pipeline) override;

		gr::TextureTargetSet& GetFinalTextureTargetSet() override
			{ return m_directionalShadowStage.GetTextureTargetSet(); }
		gr::TextureTargetSet const& GetFinalTextureTargetSet() const override 
			{ return m_directionalShadowStage.GetTextureTargetSet(); }

	private:
		void CreateBatches() override;

	private:
		gr::RenderStage m_directionalShadowStage;
		std::vector<std::shared_ptr<gr::RenderStage>> m_pointLightShadowStages; // 1 stage per light
	};
}