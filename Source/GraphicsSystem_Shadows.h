// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Camera.h"
#include "GraphicsSystem.h"
#include "ShadowMap.h"


namespace gr
{
	class ShadowsGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<ShadowsGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "Shadows"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(ShadowsGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(ShadowsGraphicsSystem, PreRender))
			);
		}

		void RegisterTextureInputs() override {};
		void RegisterTextureOutputs() override {};


	public:
		ShadowsGraphicsSystem(gr::GraphicsSystemManager*);
		~ShadowsGraphicsSystem() override = default;

		void InitPipeline(re::StagePipeline&, TextureDependencies const&);

		void PreRender();

		re::Texture const* GetShadowMap(gr::Light::Type, gr::RenderDataID) const;


	private:
		void CreateBatches();


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
}