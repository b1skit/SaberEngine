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

		static constexpr char const* k_ssaoInput = "SSAOTex";
		static constexpr char const* k_pointLightCullingInput = "PointLightCullingResults";
		static constexpr char const* k_spotLightCullingInput = "SpotLightCullingResults";
		static constexpr char const* k_shadowTexturesInput = "ShadowTextures";
		// Note: The DeferredLightingGraphicsSystem uses the same names as the GBuffer's outputs for its remaining inputs
		void RegisterInputs() override;

		static constexpr char const* k_lightOutput = "DeferredLightTarget";
		void RegisterOutputs() override;


	public:
		DeferredLightingGraphicsSystem(gr::GraphicsSystemManager*);

		~DeferredLightingGraphicsSystem() override = default;

		void InitializeResourceGenerationStages(re::StagePipeline&, TextureDependencies const&);
		void InitPipeline(re::StagePipeline&, TextureDependencies const&);

		void PreRender(DataDependencies const&);


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
			std::shared_ptr<re::Buffer> m_ambientParams;
			std::shared_ptr<re::Texture> m_IEMTex;
			std::shared_ptr<re::Texture> m_PMREMTex;
			re::Batch m_batch;
		};
		std::unordered_map<gr::RenderDataID, AmbientLightRenderData> m_ambientLightData;

		std::shared_ptr<re::RenderStage> m_ambientStage;
		std::shared_ptr<re::Buffer> m_ambientParams;
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
			std::shared_ptr<re::Buffer> m_lightParams;
			std::shared_ptr<re::Buffer> m_transformParams;
			re::Batch m_batch;
			bool m_canContribute = true;
		};
		std::unordered_map<gr::RenderDataID, PunctualLightRenderData> m_punctualLightData;

		std::shared_ptr<re::RenderStage> m_directionalStage;
		std::shared_ptr<re::RenderStage> m_pointStage;
		std::shared_ptr<re::RenderStage> m_spotStage;

	private: // Common:
		std::shared_ptr<re::TextureTargetSet> m_lightingTargetSet;


	private:
		void CreateBatches(DataDependencies const&);
	};
}