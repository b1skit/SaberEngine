// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


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

		static constexpr util::CHashKey k_directionalLightDataBufferInput = "DirectionalLightDataBuffer";
		static constexpr util::CHashKey k_pointLightDataBufferInput = "PointLightDataBuffer";
		static constexpr util::CHashKey k_spotLightDataBufferInput = "SpotLightDataBuffer";

		static constexpr util::CHashKey k_IDToPointIdxDataInput = "RenderDataIDToPointBufferIdxMap";
		static constexpr util::CHashKey k_IDToSpotIdxDataInput = "RenderDataIDToSpotBufferIdxMap";

		static constexpr util::CHashKey k_directionalShadowArrayTexInput = "DirectionalShadowArrayTex";
		static constexpr util::CHashKey k_pointShadowArrayTexInput = "PointShadowArrayTex";
		static constexpr util::CHashKey k_spotShadowArrayTexInput = "SpotShadowArrayTex";

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

		std::shared_ptr<re::Buffer> const* m_directionalLightDataBuffer;
		std::shared_ptr<re::Buffer> const* m_pointLightDataBuffer;
		std::shared_ptr<re::Buffer> const* m_spotLightDataBuffer;

		LightDataBufferIdxMap const* m_pointLightDataBufferIdxMap;
		LightDataBufferIdxMap const* m_spotLightDataBufferIdxMap;

		core::InvPtr<re::Texture> const* m_directionalShadowArrayTex;
		core::InvPtr<re::Texture> const* m_pointShadowArrayTex;
		core::InvPtr<re::Texture> const* m_spotShadowArrayTex;

		std::shared_ptr<re::Buffer> const* m_PCSSSampleParamsBuffer;
	};
}