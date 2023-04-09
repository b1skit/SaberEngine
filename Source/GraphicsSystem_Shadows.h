// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "GraphicsSystem.h"


namespace gr
{
	class ShadowsGraphicsSystem final : public virtual GraphicsSystem
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
		std::vector<gr::Camera*> m_pointLightShadowStageCams; // 1 cam per stage
	};
}