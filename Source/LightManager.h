// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "LightParamsHelpers.h"
#include "RenderObjectIDs.h"


namespace re
{
	class Buffer;
	
	struct TextureView;
}

namespace gr
{
	class RenderDataManager;


	class LightManager
	{
	public:
		static constexpr uint32_t k_invalidShadowIndex = std::numeric_limits<uint32_t>::max();


	public:
		LightManager() = default;
		~LightManager() = default;
		
		LightManager(LightManager&&) = default;
		LightManager& operator=(LightManager&&) = default;

		void Destroy();


	public:
		void UpdateLightBuffers(gr::RenderDataManager const&); // Called once at the start of each frame


	public:
		std::shared_ptr<re::Buffer> GetLightDataBuffer(gr::Light::Type) const; // Get the monolithic light data buffer


	public:
		// Deferred light volumes: Single-frame buffer containing the indexes of a single light
		std::shared_ptr<re::Buffer> GetLightIndexDataBuffer(
			gr::Light::Type, gr::RenderDataID, char const* shaderName) const;


	public:
		std::shared_ptr<re::Texture> GetShadowArrayTexture(gr::Light::Type) const;
		re::TextureView GetShadowArrayReadView(gr::Light::Type, gr::RenderDataID lightID) const;
		re::TextureView GetShadowArrayWriteView(gr::Light::Type, gr::RenderDataID lightID) const;

		// Get the logical array index (i.e. i * 6 = index of 2DArray face for a cubemap)
		uint32_t GetShadowArrayIndex(gr::Light::Type, gr::RenderDataID lightID) const;

	
	public:
		void ShowImGuiWindow() const;


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
			uint32_t m_numLights;
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
			uint32_t m_numShadows;
		};
		ShadowMetadata m_directionalShadowMetadata;
		ShadowMetadata m_pointShadowMetadata;
		ShadowMetadata m_spotShadowMetadata;

		uint32_t GetShadowArrayIndex(ShadowMetadata const&, gr::RenderDataID) const;


	private:
		void RemoveDeletedLights(gr::RenderDataManager const&);
		void RegisterNewLights(gr::RenderDataManager const& renderData);
		void UpdateLightBufferData(gr::RenderDataManager const& renderData);


	private: // No copying allowed
		LightManager(LightManager const&) = delete;
		LightManager& operator=(LightManager const&) = delete;
	};


	inline std::shared_ptr<re::Texture> LightManager::GetShadowArrayTexture(gr::Light::Type lightType) const
	{
		switch (lightType)
		{
			case gr::Light::Directional: return m_directionalShadowMetadata.m_shadowArray;
			case gr::Light::Point: return m_pointShadowMetadata.m_shadowArray;
			case gr::Light::Spot: return m_spotShadowMetadata.m_shadowArray;
			case gr::Light::AmbientIBL:
			default: SEAssertF("Invalid light type");
		}
		return nullptr; // This should never happen
	}
}