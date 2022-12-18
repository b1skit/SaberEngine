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

		std::shared_ptr<re::TextureTargetSet> GetFinalTextureTargetSet() const override;

	private:
		void CreateBatches() override;

	private:
		re::RenderStage m_directionalShadowStage;
		bool m_hasDirectionalLight;
		std::vector<std::shared_ptr<re::RenderStage>> m_pointLightShadowStages; // 1 stage per light
	};
}