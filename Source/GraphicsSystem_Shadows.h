// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"
#include "ShadowMap.h"


namespace gr
{
	class ShadowsGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		ShadowsGraphicsSystem(gr::GraphicsSystemManager*);

		void Create(re::StagePipeline&);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;

		std::vector<gr::ShadowMap::RenderData>& GetRenderData(gr::Light::LightType);
		gr::ShadowMap::RenderData const& GetShadowRenderData(gr::Light::LightType, gr::LightID) const;
		re::Texture const* GetShadowMap(gr::Light::LightType, gr::LightID) const;


	private:
		void CreateBatches() override;

	private:
		std::shared_ptr<re::RenderStage> m_directionalShadowStage;
		bool m_hasDirectionalLight;
		std::shared_ptr<re::ParameterBlock> m_directionalShadowCamPB;

		std::array<
			std::unordered_map<gr::LightID, std::shared_ptr<re::TextureTargetSet>>, 
				gr::Light::LightType::LightType_Count> m_shadowTargetSets;

		std::vector<std::shared_ptr<re::RenderStage>> m_pointLightShadowStages; // 1 stage per light
		std::vector<std::shared_ptr<re::ParameterBlock>> m_cubemapShadowParamBlocks;

		std::array<std::vector<gr::ShadowMap::RenderData>, gr::Light::LightType_Count> m_renderData;
	};


	inline std::vector<gr::ShadowMap::RenderData>& ShadowsGraphicsSystem::GetRenderData(gr::Light::LightType type)
	{
		return m_renderData[static_cast<uint8_t>(type)];
	}
}