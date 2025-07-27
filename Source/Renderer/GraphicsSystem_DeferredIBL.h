// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"
#include "GraphicsSystemCommon.h"
#include "GraphicsSystemManager.h"
#include "RenderPipeline.h"

#include "Core/InvPtr.h"

#include "Core/Util/CHashKey.h"


namespace re
{
	class MeshPrimitive;
	class Texture;
	class TextureTargetSet;
}

namespace gr
{
	class DeferredIBLGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<DeferredIBLGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "DeferredIBL"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE
				(
					INIT_PIPELINE_FN(DeferredIBLGraphicsSystem, InitializeResourceGenerationStages),
					INIT_PIPELINE_FN(DeferredIBLGraphicsSystem, InitPipeline)
				)
				PRE_RENDER(PRE_RENDER_FN(DeferredIBLGraphicsSystem, PreRender))
			);
		}

		static constexpr util::CHashKey k_lightingTargetTexInput = "LightTargetTex";
		static constexpr util::CHashKey k_AOTexInput = "AOTex";

		// Note: The DeferredIBLGraphicsSystem uses GBufferGraphicsSystem::GBufferTexNames for its remaining inputs
		void RegisterInputs() override;

		static constexpr util::CHashKey k_activeAmbientIEMTexOutput = "ActiveAmbientIEMTex";
		static constexpr util::CHashKey k_activeAmbientPMREMTexOutput = "ActiveAmbientPMREMTex";
		static constexpr util::CHashKey k_activeAmbientDFGTexOutput = "ActiveAmbientDFGTex";
		static constexpr util::CHashKey k_activeAmbientParamsBufferOutput = "ActiveAmbientParamsBuffer";
		void RegisterOutputs() override;


	public:
		DeferredIBLGraphicsSystem(gr::GraphicsSystemManager*);

		~DeferredIBLGraphicsSystem() override = default;

		void InitializeResourceGenerationStages(
			gr::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);

		void InitPipeline(
			gr::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);

		void PreRender();


	private:
		// BRDF Pre-integration:
		void CreateSingleFrameBRDFPreIntegrationStage(gr::StagePipeline&);
		core::InvPtr<re::Texture> m_BRDF_integrationMap;


	private:
		// Ambient IBL resources:
		// TODO: We should use equirectangular images, instead of bothering to convert to cubemaps for IEM/PMREM
		// -> Need to change the HLSL Get___DominantDir functions to ensure the result is normalized
		void PopulateIEMTex(
			gr::StagePipeline*, core::InvPtr<re::Texture> const& iblTex, core::InvPtr<re::Texture>& iemTexOut) const;

		void PopulatePMREMTex(
			gr::StagePipeline*, core::InvPtr<re::Texture> const& iblTex, core::InvPtr<re::Texture>& pmremTexOut) const;


	private:
		void CreateBatches();


	private: // Ambient lights:
		struct AmbientLightRenderData
		{
			std::shared_ptr<re::Buffer> m_ambientParams;
			core::InvPtr<re::Texture> m_IEMTex;
			core::InvPtr<re::Texture> m_PMREMTex;
			gr::BatchHandle m_batch;
		};
		std::unordered_map<gr::RenderDataID, AmbientLightRenderData> m_ambientLightData;

		// We maintain pointer-stable copies of the active ambient light params so they can be shared with other GS's
		struct ActiveAmbientRenderData
		{
			gr::RenderDataID m_renderDataID = gr::k_invalidRenderDataID;
			std::shared_ptr<re::Buffer> m_ambientParams;
			core::InvPtr<re::Texture> m_IEMTex;
			core::InvPtr<re::Texture> m_PMREMTex;
		} m_activeAmbientLightData;

		std::shared_ptr<gr::Stage> m_ambientStage;
		re::BufferInput m_ambientParams;
		core::InvPtr<re::Texture> m_AOTex;

		std::shared_ptr<re::TextureTargetSet> m_lightingTargetSet;

		gr::StagePipeline* m_resourceCreationStagePipeline;
		gr::StagePipeline::StagePipelineItr m_resourceCreationStageParentItr;

		// For rendering into a cube map (IEM/PMREM generation)
		core::InvPtr<gr::MeshPrimitive> m_cubeMeshPrimitive;
		gr::BatchHandle m_cubeMeshBatch;
		std::array<std::shared_ptr<re::Buffer>, 6> m_cubemapRenderCamParams;	
	};
}