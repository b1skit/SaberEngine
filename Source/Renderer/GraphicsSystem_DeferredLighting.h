// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"
#include "LightRenderData.h"


namespace re
{
	class MeshPrimitive;
}


namespace gr
{
	class ShadowsGraphicsSystem;
	class XeGTAOGraphicsSystem;


	class DeferredLightingGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<DeferredLightingGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "DeferredLighting"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE
				(
					INIT_PIPELINE_FN(DeferredLightingGraphicsSystem, InitializeResourceGenerationStages),
					INIT_PIPELINE_FN(DeferredLightingGraphicsSystem, InitPipeline)
				)
				PRE_RENDER(PRE_RENDER_FN(DeferredLightingGraphicsSystem, PreRender))
			);
		}

		static constexpr util::HashKey k_ssaoInput = "SSAOTex";
		static constexpr util::HashKey k_pointLightCullingDataInput = "PointLightCullingResults";
		static constexpr util::HashKey k_spotLightCullingDataInput = "SpotLightCullingResults";
		// Note: The DeferredLightingGraphicsSystem uses GBufferGraphicsSystem::GBufferTexNames for its remaining inputs
		void RegisterInputs() override;

		static constexpr util::HashKey k_lightingTexOutput = "DeferredLightTarget";
		static constexpr util::HashKey k_activeAmbientIEMTexOutput = "ActiveAmbientIEMTex";
		static constexpr util::HashKey k_activeAmbientPMREMTexOutput = "ActiveAmbientPMREMTex";
		static constexpr util::HashKey k_activeAmbientDFGTexOutput = "ActiveAmbientDFGTex";
		static constexpr util::HashKey k_activeAmbientParamsBufferOutput = "ActiveAmbientParamsBuffer";
		void RegisterOutputs() override;


	public:
		DeferredLightingGraphicsSystem(gr::GraphicsSystemManager*);

		~DeferredLightingGraphicsSystem() override = default;

		void InitializeResourceGenerationStages(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);

		void PreRender();


	private:
		// BRDF Pre-integration:
		void CreateSingleFrameBRDFPreIntegrationStage(re::StagePipeline&);
		std::shared_ptr<re::Texture> m_BRDF_integrationMap;

	private:
		// Ambient IBL resources:
		// TODO: We should use equirectangular images, instead of bothering to convert to cubemaps for IEM/PMREM
		// -> Need to change the HLSL Get___DominantDir functions to ensure the result is normalized
		void PopulateIEMTex(
			re::StagePipeline*, re::Texture const* iblTex, std::shared_ptr<re::Texture>& iemTexOut) const;

		void PopulatePMREMTex(
			re::StagePipeline*, re::Texture const* iblTex, std::shared_ptr<re::Texture>& pmremTexOut) const;

	private: // Ambient lights:
		struct AmbientLightRenderData
		{
			re::BufferInput m_ambientParams;
			std::shared_ptr<re::Texture> m_IEMTex;
			std::shared_ptr<re::Texture> m_PMREMTex;
			re::Batch m_batch;
		};
		std::unordered_map<gr::RenderDataID, AmbientLightRenderData> m_ambientLightData;

		// We maintain pointer-stable copies of the active ambient light params so they can be shared with other GS's
		struct ActiveAmbientRenderData
		{
			gr::RenderDataID m_renderDataID = gr::k_invalidRenderDataID;
			re::BufferInput m_ambientParams;
			std::shared_ptr<re::Texture> m_IEMTex;
			std::shared_ptr<re::Texture> m_PMREMTex;
		} m_activeAmbientLightData;

		std::shared_ptr<re::RenderStage> m_ambientStage;
		re::BufferInput m_ambientParams;
		std::shared_ptr<re::Texture> m_ssaoTex;

		re::StagePipeline* m_resourceCreationStagePipeline;
		re::StagePipeline::StagePipelineItr m_resourceCreationStageParentItr;

		// For rendering into a cube map (IEM/PMREM generation)
		std::shared_ptr<gr::MeshPrimitive> m_cubeMeshPrimitive;
		std::unique_ptr<re::Batch> m_cubeMeshBatch;
		std::array<std::shared_ptr<re::Buffer>, 6> m_cubemapRenderCamParams;


	private: // Punctual lights:
		struct PunctualLightRenderData
		{
			gr::Light::Type m_type;
			re::BufferInput m_transformParams;
			re::Batch m_batch;
			bool m_hasShadow = false;
			bool m_canContribute = true;
		};
		std::unordered_map<gr::RenderDataID, PunctualLightRenderData> m_punctualLightData;

		std::shared_ptr<re::RenderStage> m_directionalStage;
		std::shared_ptr<re::RenderStage> m_pointStage;
		std::shared_ptr<re::RenderStage> m_spotStage;

	private: // Common:
		std::shared_ptr<re::TextureTargetSet> m_lightingTargetSet;
		
		std::shared_ptr<re::Texture> m_missing2DShadowFallback;
		std::shared_ptr<re::Texture> m_missingCubeShadowFallback;

	private:
		void CreateBatches();


	private: // Cached dependencies:
		PunctualLightCullingResults const* m_pointCullingResults;
		PunctualLightCullingResults const* m_spotCullingResults;
	};
}