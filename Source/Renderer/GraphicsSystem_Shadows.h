// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "BufferView.h"
#include "CameraRenderData.h"
#include "GraphicsSystem.h"
#include "ShadowMapRenderData.h"
#include "TransformRenderData.h"

#include "Core/InvPtr.h"


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

		static constexpr util::CHashKey k_lightIDToShadowRecordInput = "LightIDToShadowRecordMap";

		void RegisterInputs() override;


		// Shadow array textures:
		static constexpr util::CHashKey k_directionalShadowArrayTexOutput = "DirectionalShadowArrayTex";
		static constexpr util::CHashKey k_pointShadowArrayTexOutput = "PointShadowArrayTex";
		static constexpr util::CHashKey k_spotShadowArrayTexOutput = "SpotShadowArrayTex";

		// Maps from light RenderDataID to gr::ShadowRecord:
		static constexpr util::CHashKey k_lightIDToShadowRecordOutput = "LightIDToShadowRecordMap";

		static constexpr util::CHashKey k_PCSSSampleParamsBufferOutput = "PCSSSampleParamsBuffer";

		void RegisterOutputs() override;


	public:
		ShadowsGraphicsSystem(gr::GraphicsSystemManager*);
		~ShadowsGraphicsSystem() override = default;

		void InitPipeline(gr::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void PreRender();


	public:
		void ShowImGuiWindow() override;


	private:
		void CreateBatches();


	private:
		struct ShadowStageData
		{
			std::shared_ptr<gr::Stage> m_clearStage;
			std::shared_ptr<gr::Stage> m_stage;
			std::shared_ptr<re::TextureTargetSet> m_shadowTargetSet;
			re::BufferInput m_shadowRenderCameraParams;

			gr::Light::Type m_lightType;
		};
		std::unordered_map<gr::RenderDataID, ShadowStageData> m_shadowStageData;


		void CreateRegister2DShadowStage(
			std::unordered_map<gr::RenderDataID, ShadowStageData>& dstStageData,
			gr::RenderDataID,
			gr::Light::Type,
			gr::ShadowMap::RenderData const&,
			gr::Camera::RenderData const&);


		void CreateRegisterCubeShadowStage(
			std::unordered_map<gr::RenderDataID, ShadowStageData>& dstStageData,
			gr::RenderDataID,
			gr::Light::Type,
			gr::ShadowMap::RenderData const&,
			gr::Transform::RenderData const&,
			gr::Camera::RenderData const&);


		void RegisterNewShadowStages();
		void UpdateShadowStages();


	private:
		// Pipeline:
		gr::StagePipeline* m_stagePipeline;
		gr::StagePipeline::StagePipelineItr m_directionalParentStageItr;
		gr::StagePipeline::StagePipelineItr m_pointParentStageItr;
		gr::StagePipeline::StagePipelineItr m_spotParentStageItr;


	private: // Dependency inputs:
		PunctualLightCullingResults const* m_pointCullingResults;
		PunctualLightCullingResults const* m_spotCullingResults;
		
		ViewBatches const* m_viewBatches;
		AllBatches const* m_allBatches;


	private:
		// Percentage delta from the current no. buffer elements (i.e. high-water mark) to the current # lights that 
		// will trigger a reallocation to a smaller buffer
		static constexpr float k_shrinkReallocationFactor = 0.5f;


	private: // Shadow texture array management:
		struct ShadowTextureMetadata
		{
			std::unordered_map<gr::RenderDataID, uint32_t> m_renderDataIDToTexArrayIdx;
			std::map<uint32_t, gr::RenderDataID> m_texArrayIdxToRenderDataID;

			core::InvPtr<re::Texture> m_shadowArray;
			uint32_t m_numShadows = 0;
		};
		ShadowTextureMetadata m_directionalShadowTexMetadata;
		ShadowTextureMetadata m_pointShadowTexMetadata;
		ShadowTextureMetadata m_spotShadowTexMetadata;

		// Data outputs:
		std::unordered_map<gr::RenderDataID, gr::ShadowRecord> m_lightIDToShadowRecords;
		std::shared_ptr<re::Buffer> m_poissonSampleParamsBuffer;

	private:
		// Get the logical array index (i.e. i * 6 = index of 2DArray face for a cubemap)
		uint32_t GetShadowArrayIndex(ShadowTextureMetadata const&, gr::RenderDataID) const;
		uint32_t GetShadowArrayIndex(gr::Light::Type, gr::RenderDataID lightID) const;


	private:
		void RemoveDeletedShadowRecords(gr::RenderDataManager const&);
		void RegisterNewShadowTextureElements(gr::RenderDataManager const& renderData);
		void UpdateShadowTextures(gr::RenderDataManager const& renderData);
	};
}