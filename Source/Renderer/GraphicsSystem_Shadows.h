// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "CameraRenderData.h"
#include "GraphicsSystem.h"
#include "ShadowMapRenderData.h"


namespace core
{
	template<typename T>
	class InvPtr;
}

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

		static constexpr util::CHashKey k_pointLightCullingDataInput = "PointLightCullingResults";
		static constexpr util::CHashKey k_spotLightCullingDataInput = "SpotLightCullingResults";

		static constexpr util::CHashKey k_viewBatchesDataInput = "ViewBatches";
		static constexpr util::CHashKey k_allBatchesDataInput = "AllBatches";

		static constexpr util::CHashKey k_directionalShadowArrayTexInput = "DirectionalShadowArrayTex";
		static constexpr util::CHashKey k_pointShadowArrayTexInput = "PointShadowArrayTex";
		static constexpr util::CHashKey k_spotShadowArrayTexInput = "SpotShadowArrayTex";

		static constexpr util::CHashKey k_IDToDirectionalShadowArrayIdxDataInput = "RenderDataIDToDirectionalShadowArrayIdxMap";
		static constexpr util::CHashKey k_IDToPointShadowArrayIdxDataInput = "RenderDataIDToPointShadowArrayIdxMap";
		static constexpr util::CHashKey k_IDToSpotShadowArrayIdxDataInput = "RenderDataIDToSpotShadowArrayIdxMap";

		void RegisterInputs() override;

		static constexpr util::CHashKey k_shadowTexturesOutput = "ShadowTextures";
		void RegisterOutputs() override;


	public:
		ShadowsGraphicsSystem(gr::GraphicsSystemManager*);
		~ShadowsGraphicsSystem() override = default;

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void PreRender();


	private:
		void CreateBatches();


	private:
		struct ShadowStageData
		{
			std::shared_ptr<re::RenderStage> m_renderStage;
			std::shared_ptr<re::TextureTargetSet> m_shadowTargetSet;
			re::BufferInput m_shadowCamParamBlock;
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


	private: // Cached dependencies:
		PunctualLightCullingResults const* m_pointCullingResults;
		PunctualLightCullingResults const* m_spotCullingResults;
		
		ViewBatches const* m_viewBatches;
		AllBatches const* m_allBatches;

		core::InvPtr<re::Texture> const* m_directionalShadowArrayTex;
		core::InvPtr<re::Texture> const* m_pointShadowArrayTex;
		core::InvPtr<re::Texture> const* m_spotShadowArrayTex;

		ShadowArrayIdxMap const* m_directionalShadowArrayIdxMap;
		ShadowArrayIdxMap const* m_pointShadowArrayIdxMap;
		ShadowArrayIdxMap const* m_spotShadowArrayIdxMap;
	};
}