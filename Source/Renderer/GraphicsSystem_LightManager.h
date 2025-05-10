// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"
#include "LightRenderData.h"


namespace gr
{
	class RenderDataManager;


	class LightManagerGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<LightManagerGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "LightManager"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(LightManagerGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(LightManagerGraphicsSystem, PreRender))
			);
		}

		void RegisterInputs() override;

		// Shadow array textures:
		static constexpr util::CHashKey k_directionalShadowArrayTexOutput = "DirectionalShadowArrayTex";
		static constexpr util::CHashKey k_pointShadowArrayTexOutput = "PointShadowArrayTex";
		static constexpr util::CHashKey k_spotShadowArrayTexOutput = "SpotShadowArrayTex";

		// Maps from light RenderDataID to shadow records:
		static constexpr util::CHashKey k_lightIDToShadowRecordOutput = "LightIDToShadowRecordMap";
		static constexpr util::CHashKey k_PCSSSampleParamsBufferOutput = "PCSSSampleParamsBuffer";

		void RegisterOutputs() override;


	public:
		LightManagerGraphicsSystem(gr::GraphicsSystemManager*);

		~LightManagerGraphicsSystem() override = default;

		void InitPipeline(
			re::StagePipeline& pipeline, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);

		void PreRender();


	public:
		void ShowImGuiWindow() override;


	private:
		// Percentage delta from the current no. buffer elements (i.e. high-water mark) to the current # lights that 
		// will trigger a reallocation to a smaller buffer
		static constexpr float k_shrinkReallocationFactor = 0.5f;


	private: // Shadow management:
		struct ShadowMetadata
		{
			std::unordered_map<gr::RenderDataID, uint32_t> m_renderDataIDToTexArrayIdx;
			std::map<uint32_t, gr::RenderDataID> m_texArrayIdxToRenderDataID;

			core::InvPtr<re::Texture> m_shadowArray;
			uint32_t m_numShadows = 0;
		};
		ShadowMetadata m_directionalShadowMetadata;
		ShadowMetadata m_pointShadowMetadata;
		ShadowMetadata m_spotShadowMetadata;
		
		std::unordered_map<gr::RenderDataID, gr::ShadowRecord> m_lightIDToShadowRecords;

		std::shared_ptr<re::Buffer> m_poissonSampleParamsBuffer;


	private:
		// Get the logical array index (i.e. i * 6 = index of 2DArray face for a cubemap)
		uint32_t GetShadowArrayIndex(ShadowMetadata const&, gr::RenderDataID) const;
		uint32_t GetShadowArrayIndex(gr::Light::Type, gr::RenderDataID lightID) const;


	private:
		void RemoveDeletedLights(gr::RenderDataManager const&);
		void RegisterNewLights(gr::RenderDataManager const& renderData);
		void UpdateLightBufferData(gr::RenderDataManager const& renderData);
	};
}