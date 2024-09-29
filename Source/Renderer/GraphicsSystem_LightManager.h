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


		// Monolithic light data buffers:
		// NOTE: These buffers may be reallocated; They must be attached every frame as a single frame input ONLY
		static constexpr util::HashKey k_directionalLightDataBufferOutput = "DirectionalLightDataBuffer";
		static constexpr util::HashKey k_pointLightDataBufferOutput = "PointLightDataBuffer";
		static constexpr util::HashKey k_spotLightDataBufferOutput = "SpotLightDataBuffer";

		// Maps from RenderDataID to monolithic light data buffer indexes:
		static constexpr util::HashKey k_IDToDirectionalIdxDataOutput = "RenderDataIDToDirectionalBufferIdxMap";
		static constexpr util::HashKey k_IDToPointIdxDataOutput = "RenderDataIDToPointBufferIdxMap";
		static constexpr util::HashKey k_IDToSpotIdxDataOutput = "RenderDataIDToSpotBufferIdxMap";

		// Shadow array textures:
		// Note: Textures may be reallocated at the start of any frame; Texture inputs should be reset each frame
		static constexpr util::HashKey k_directionalShadowArrayTexOutput = "DirectionalShadowArrayTex";
		static constexpr util::HashKey k_pointShadowArrayTexOutput = "PointShadowArrayTex";
		static constexpr util::HashKey k_spotShadowArrayTexOutput = "SpotShadowArrayTex";

		// Maps from RenderDataID to shadow array texture indexes:
		static constexpr util::HashKey k_IDToDirectionalShadowArrayIdxDataOutput = "RenderDataIDToDirectionalShadowArrayIdxMap";
		static constexpr util::HashKey k_IDToPointShadowArrayIdxDataOutput = "RenderDataIDToPointShadowArrayIdxMap";
		static constexpr util::HashKey k_IDToSpotShadowArrayIdxDataOutput = "RenderDataIDToSpotShadowArrayIdxMap";

		static constexpr util::HashKey k_PCSSSampleParamsBufferOutput = "PCSSSampleParamsBuffer";

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


	private: // Light management:
		struct LightMetadata
		{
			std::unordered_map<gr::RenderDataID, uint32_t> m_renderDataIDToBufferIdx;
			std::map<uint32_t, gr::RenderDataID> m_bufferIdxToRenderDataID;

			std::vector<uint32_t> m_dirtyMovedIndexes; // Light entries that were moved during per-frame deletion

			std::shared_ptr<re::Buffer> m_lightData; // Always has at least 1 element (i.e. a dummy if no lights exist)
			uint32_t m_numLights = 0;
		};
		LightMetadata m_directionalLightMetadata;
		LightMetadata m_pointLightMetadata;
		LightMetadata m_spotLightMetadata;


	private: // Shadow management:
		struct ShadowMetadata
		{
			std::unordered_map<gr::RenderDataID, uint32_t> m_renderDataIDToTexArrayIdx;
			std::map<uint32_t, gr::RenderDataID> m_texArrayIdxToRenderDataID;

			std::shared_ptr<re::Texture> m_shadowArray;
			uint32_t m_numShadows = 0;
		};
		ShadowMetadata m_directionalShadowMetadata;
		ShadowMetadata m_pointShadowMetadata;
		ShadowMetadata m_spotShadowMetadata;

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