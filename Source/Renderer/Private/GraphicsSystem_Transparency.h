// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"
#include "Renderer/Shaders/Common/LightParams.h"


namespace gr
{
	class TransparencyGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<TransparencyGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "Transparency"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(TransparencyGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(TransparencyGraphicsSystem, PreRender))
			);
		}

		static constexpr util::CHashKey k_sceneDepthTexInput = "SceneDepth";
		static constexpr util::CHashKey k_sceneLightingTexInput = "SceneLightingTarget";

		static constexpr util::CHashKey k_pointLightCullingDataInput = "PointLightCullingResults";
		static constexpr util::CHashKey k_spotLightCullingDataInput = "SpotLightCullingResults";

		static constexpr util::CHashKey k_viewBatchesDataInput = "ViewBatches";
		static constexpr util::CHashKey k_allBatchesDataInput = "AllBatches";

		static constexpr util::CHashKey k_ambientIEMTexInput = "AmbientIEMTex";
		static constexpr util::CHashKey k_ambientPMREMTexInput = "AmbientPMREMTex";
		static constexpr util::CHashKey k_ambientDFGTexInput = "AmbientDFGTex";
		static constexpr util::CHashKey k_ambientParamsBufferInput = "AmbientParamsBuffer";

		static constexpr util::CHashKey k_directionalShadowArrayTexInput = "DirectionalShadowArrayTex";
		static constexpr util::CHashKey k_pointShadowArrayTexInput = "PointShadowArrayTex";
		static constexpr util::CHashKey k_spotShadowArrayTexInput = "SpotShadowArrayTex";
		
		static constexpr util::CHashKey k_lightIDToShadowRecordInput = "LightIDToShadowRecordMap";

		static constexpr util::CHashKey k_PCSSSampleParamsBufferInput = "PCSSSampleParamsBuffer";

		void RegisterInputs() override;

		void RegisterOutputs() override;


	public:
		TransparencyGraphicsSystem(gr::GraphicsSystemManager*);

		~TransparencyGraphicsSystem() override = default;

		void InitPipeline(re::StagePipeline& pipeline, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);

		void PreRender();


	private:
		std::shared_ptr<re::Stage> m_transparencyStage;

	private: // Cached dependencies:
		core::InvPtr<re::Texture> const* m_ambientIEMTex;
		core::InvPtr<re::Texture> const* m_ambientPMREMTex;
		std::shared_ptr<re::Buffer> const* m_ambientParams;

		PunctualLightCullingResults const* m_pointCullingResults;
		PunctualLightCullingResults const* m_spotCullingResults;

		ViewBatches const* m_viewBatches;
		AllBatches const* m_allBatches;

		core::InvPtr<re::Texture> const* m_directionalShadowArrayTex;
		core::InvPtr<re::Texture> const* m_pointShadowArrayTex;
		core::InvPtr<re::Texture> const* m_spotShadowArrayTex;

		std::unordered_map<gr::RenderDataID, gr::ShadowRecord> const* m_lightIDToShadowRecords;

		LightMetadata m_lightMetadata;
		re::BufferInput m_lightMetadataBuffer;

		std::shared_ptr<re::Buffer> const* m_PCSSSampleParamsBuffer;
	};
}