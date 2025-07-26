// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"
#include "GraphicsSystemCommon.h"
#include "LightRenderData.h"

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
	class StagePipeline;


	class DeferredLightVolumeGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<DeferredLightVolumeGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "DeferredLightVolumes"; }


		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE
				(
					INIT_PIPELINE_FN(DeferredLightVolumeGraphicsSystem, InitPipeline)
				)
				PRE_RENDER(PRE_RENDER_FN(DeferredLightVolumeGraphicsSystem, PreRender))
			);
		}

		static constexpr util::CHashKey k_lightingTargetTexInput = "LightTargetTex";
		static constexpr util::CHashKey k_pointLightCullingDataInput = "PointLightCullingResults";
		static constexpr util::CHashKey k_spotLightCullingDataInput = "SpotLightCullingResults";

		static constexpr util::CHashKey k_lightIDToShadowRecordInput = "LightIDToShadowRecordMap";
		static constexpr util::CHashKey k_PCSSSampleParamsBufferInput = "PCSSSampleParamsBuffer";

		// Note: The DeferredLightVolumeGraphicsSystem uses GBufferGraphicsSystem::GBufferTexNames for its remaining inputs
		void RegisterInputs() override;

		void RegisterOutputs() override;


	public:
		DeferredLightVolumeGraphicsSystem(gr::GraphicsSystemManager*);

		~DeferredLightVolumeGraphicsSystem() override = default;

		void InitPipeline(
			gr::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);

		void PreRender();


	private: // Punctual lights:
		struct PunctualLightRenderData
		{
			gr::Light::Type m_type;
			gr::BatchHandle m_batch;
			bool m_hasShadow = false;
			bool m_canContribute = true;
		};
		std::unordered_map<gr::RenderDataID, PunctualLightRenderData> m_punctualLightData;

		std::shared_ptr<gr::Stage> m_directionalStage;
		std::shared_ptr<gr::Stage> m_pointStage;
		std::shared_ptr<gr::Stage> m_spotStage;

	private: // Common:
		std::shared_ptr<re::TextureTargetSet> m_lightingTargetSet;
		
		core::InvPtr<re::Texture> m_missing2DShadowFallback;
		core::InvPtr<re::Texture> m_missingCubeShadowFallback;


	private: // Cached dependencies:
		PunctualLightCullingResults const* m_pointCullingResults;
		PunctualLightCullingResults const* m_spotCullingResults;

		LightIDToShadowRecordMap const* m_lightIDToShadowRecords;
		std::shared_ptr<re::Buffer> const* m_PCSSSampleParamsBuffer;

		core::InvPtr<re::Texture> const* m_lightingTargetTex;


	private:
		void CreateBatches();


	private:
		void HandleEvents() override;

		bool m_directionalShadowTexArrayUpdated;
		bool m_pointShadowTexArrayUpdated;
		bool m_spotShadowTexArrayUpdated;
	};
}