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

		static constexpr util::HashKey k_sceneDepthTexInput = "SceneDepth";
		static constexpr util::HashKey k_sceneLightingTexInput = "SceneLightingTarget";

		static constexpr util::HashKey k_pointLightCullingDataInput = "PointLightCullingResults";
		static constexpr util::HashKey k_spotLightCullingDataInput = "SpotLightCullingResults";

		static constexpr util::HashKey k_viewBatchesDataInput = "ViewBatches";
		static constexpr util::HashKey k_allBatchesDataInput = "AllBatches";

		static constexpr util::HashKey k_ambientIEMTexInput = "AmbientIEMTex";
		static constexpr util::HashKey k_ambientPMREMTexInput = "AmbientPMREMTex";
		static constexpr util::HashKey k_ambientDFGTexInput = "AmbientDFGTex";
		static constexpr util::HashKey k_ambientParamsBufferInput = "AmbientParamsBuffer";

		static constexpr util::HashKey k_directionalLightDataBufferInput = "DirectionalLightDataBuffer";
		static constexpr util::HashKey k_pointLightDataBufferInput = "PointLightDataBuffer";
		static constexpr util::HashKey k_spotLightDataBufferInput = "SpotLightDataBuffer";

		static constexpr util::HashKey k_IDToPointIdxDataInput = "RenderDataIDToPointBufferIdxMap";
		static constexpr util::HashKey k_IDToSpotIdxDataInput = "RenderDataIDToSpotBufferIdxMap";

		static constexpr util::HashKey k_directionalShadowArrayTexInput = "DirectionalShadowArrayTex";
		static constexpr util::HashKey k_pointShadowArrayTexInput = "PointShadowArrayTex";
		static constexpr util::HashKey k_spotShadowArrayTexInput = "SpotShadowArrayTex";

		static constexpr util::HashKey k_PCSSSampleParamsBufferInput = "PCSSSampleParamsBuffer";

		void RegisterInputs() override;

		void RegisterOutputs() override;


	public:
		TransparencyGraphicsSystem(gr::GraphicsSystemManager*);

		~TransparencyGraphicsSystem() override = default;

		void InitPipeline(re::StagePipeline& pipeline, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);

		void PreRender();


	private:
		std::shared_ptr<re::RenderStage> m_transparencyStage;

	private: // Cached dependencies:
		std::shared_ptr<re::Texture> const* m_ambientIEMTex;
		std::shared_ptr<re::Texture> const* m_ambientPMREMTex;
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

		std::shared_ptr<re::Texture> const* m_directionalShadowArrayTex;
		std::shared_ptr<re::Texture> const* m_pointShadowArrayTex;
		std::shared_ptr<re::Texture> const* m_spotShadowArrayTex;

		std::shared_ptr<re::Buffer> const* m_PCSSSampleParamsBuffer;
	};
}