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

		re::TextureTargetSet& GetFinalTextureTargetSet() override
			{ return m_directionalShadowStage.GetTextureTargetSet(); }
		re::TextureTargetSet const& GetFinalTextureTargetSet() const override 
			{ return m_directionalShadowStage.GetTextureTargetSet(); }

	private:
		void CreateBatches() override;

	private:
		re::RenderStage m_directionalShadowStage;
		bool m_hasDirectionalLight;
		std::vector<std::shared_ptr<re::RenderStage>> m_pointLightShadowStages; // 1 stage per light
	};
}