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
		void CreateBatches() override;


	private:
		struct ShadowStageData
		{
			std::shared_ptr<re::RenderStage> m_renderStage;
			std::shared_ptr<re::TextureTargetSet> m_shadowTargetSet;
			std::shared_ptr<re::Buffer> m_shadowCamParamBlock;
		};

		std::unordered_map<gr::RenderDataID, ShadowStageData> m_directionalShadowStageData;
		std::unordered_map<gr::RenderDataID, ShadowStageData> m_pointShadowStageData;
		std::unordered_map<gr::RenderDataID, ShadowStageData> m_spotShadowStageData;


		void CreateRegister2DShadowStage(
			std::unordered_map<gr::RenderDataID, ShadowStageData>& dstStageData,
			gr::RenderDataID,
			gr::ShadowMap::RenderData const&,
			gr::Camera::RenderData const&);


		void CreateRegisterCubeShadowStage(
			std::unordered_map<gr::RenderDataID, ShadowStageData>& dstStageData,
			gr::RenderDataID,
			gr::ShadowMap::RenderData const&,
			gr::Transform::RenderData const&,
			gr::Camera::RenderData const&);
		

	private:
		// Pipeline:
		re::StagePipeline* m_stagePipeline;
		re::StagePipeline::StagePipelineItr m_directionalParentStageItr;
		re::StagePipeline::StagePipelineItr m_pointParentStageItr;
		re::StagePipeline::StagePipelineItr m_spotParentStageItr;
	};


	inline std::shared_ptr<re::TextureTargetSet const> ShadowsGraphicsSystem::GetFinalTextureTargetSet() const
	{
		SEAssertF("The shadow graphics system has many target set output. Use GetShadowMap() instead");
		return nullptr;
	}
}