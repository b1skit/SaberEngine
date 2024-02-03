// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Camera.h"
#include "GraphicsSystem.h"
#include "ShadowMap.h"


namespace gr
{
	class ShadowsGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		ShadowsGraphicsSystem(gr::GraphicsSystemManager*);
		~ShadowsGraphicsSystem() override = default;

		void Create(re::StagePipeline&);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;

		re::Texture const* GetShadowMap(gr::Light::Type, gr::RenderDataID) const;


	private:
		std::shared_ptr<re::RenderStage> CreateRegisterDirectionalShadowStage(
			gr::RenderDataID, gr::ShadowMap::RenderData const&, gr::Camera::RenderData const&);

		std::shared_ptr<re::RenderStage> CreateRegisterPointShadowStage(
			gr::RenderDataID, 
			gr::ShadowMap::RenderData const&, 
			gr::Transform::RenderData const&, 
			gr::Camera::RenderData const&);

		void CreateBatches() override;


	private:
		struct DirectionalShadowStageData
		{
			std::shared_ptr<re::RenderStage> m_renderStage;
			std::shared_ptr<re::TextureTargetSet> m_shadowTargetSet;
			std::shared_ptr<re::ParameterBlock> m_shadowCamParamBlock;
		};
		std::unordered_map<gr::RenderDataID, DirectionalShadowStageData> m_directionalShadowStageData;

		struct PointShadowStageData
		{
			std::shared_ptr<re::RenderStage> m_renderStage;
			std::shared_ptr<re::TextureTargetSet> m_shadowTargetSet;
			std::shared_ptr<re::ParameterBlock> m_cubemapShadowParamBlock;
		};
		std::unordered_map<gr::RenderDataID, PointShadowStageData> m_pointShadowStageData;

		// Pipeline:
		re::StagePipeline* m_stagePipeline;
		re::StagePipeline::StagePipelineItr m_directionalParentStageItr;
		re::StagePipeline::StagePipelineItr m_pointParentStageItr;
	};


	inline std::shared_ptr<re::TextureTargetSet const> ShadowsGraphicsSystem::GetFinalTextureTargetSet() const
	{
		SEAssertF("The shadow graphics system has many target set output. Use GetShadowMap() instead");
		return nullptr;
	}
}