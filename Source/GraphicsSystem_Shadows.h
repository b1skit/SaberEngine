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
		~ShadowsGraphicsSystem();

		void Create(re::StagePipeline&);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;

		void RegisterShadowMap(gr::Light::Type, gr::RenderDataID);
		void UnregisterShadowMap(gr::Light::Type, gr::RenderDataID);

		re::Texture const* GetShadowMap(gr::Light::Type, gr::RenderDataID) const;


	private:
		void CreateBatches() override;

	private:
		std::shared_ptr<re::RenderStage> m_directionalShadowStage;
		bool m_hasDirectionalLight;
		std::shared_ptr<re::ParameterBlock> m_directionalShadowCamPB;

		std::array<std::vector<gr::RenderDataID>, gr::Light::Type_Count> m_shadowRenderDataIDs;

		std::array<
			std::unordered_map<gr::RenderDataID, std::shared_ptr<re::TextureTargetSet>>, 
				gr::Light::Type::Type_Count> m_shadowTargetSets;

		struct PointLightStageData
		{
			std::shared_ptr<re::RenderStage> m_renderStage;
			std::shared_ptr<re::ParameterBlock> m_cubemapShadowParamBlock;
		};
		std::unordered_map<gr::RenderDataID, PointLightStageData> m_pointLightStageData;
		// TODO: We will need to update this if lights are added/removed outside of Create()
	};
}