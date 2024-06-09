// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "LightParamsHelpers.h"
#include "RenderObjectIDs.h"


namespace re
{
	class Buffer;
}

namespace gr
{
	class RenderDataManager;


	class LightManager
	{
	public:
		LightManager() = default;
		~LightManager() = default;
		
		LightManager(LightManager&&) = default;
		LightManager& operator=(LightManager&&) = default;


	public:
		void UpdateLightBuffers(gr::RenderDataManager const&); // Called once at the start of each frame


	public:
		std::shared_ptr<re::Buffer> GetLightDataBuffer(gr::Light::Type) const; // Get the monolithic light data buffer


	public:
		// Light volumes: Single-frame buffer containing the index of a single light
		std::shared_ptr<re::Buffer> GetLightIndexDataBuffer(
			gr::Light::Type, gr::RenderDataID, char const* shaderName) const;


	private:
		// Percentage delta from the current no. buffer elements (i.e. high-water mark) to the current # lights that 
		// will trigger a reallocation to a smaller buffer
		static constexpr float k_shrinkReallocationFactor = 0.5f;

		struct LightMetadata
		{
			std::unordered_map<gr::RenderDataID, uint32_t> m_renderDataIDToBufferIdx;
			std::map<uint32_t, gr::RenderDataID> m_bufferIdxToRenderDataID;

			std::vector<uint32_t> m_dirtyMovedIndexes; // Light entries that were moved during per-frame deletion

			std::shared_ptr<re::Buffer> m_lightData; // Always has at least 1 element (i.e. a dummy if no lights exist)
			uint32_t m_numLights;
		};
		LightMetadata m_directionalMetadata;
		LightMetadata m_pointMetadata;
		LightMetadata m_spotMetadata;

		void RemoveDeletedLights(gr::RenderDataManager const&);
		void RegisterNewLights(gr::RenderDataManager const& renderData);
		void UpdateLightBufferData(gr::RenderDataManager const& renderData);


	private: // No copying allowed
		LightManager(LightManager const&) = delete;
		LightManager& operator=(LightManager const&) = delete;
	};
}