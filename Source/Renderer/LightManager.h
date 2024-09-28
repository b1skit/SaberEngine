// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "BufferInput.h"
#include "LightParamsHelpers.h"
#include "RenderObjectIDs.h"
#include "TextureView.h"


namespace re
{
	class Buffer;
	class ScissorRect;
	class Viewport;
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
		
		LightManager(LightManager&&) noexcept = default;
		LightManager& operator=(LightManager&&) noexcept = default;

		void Initialize();
		void Destroy();


	public:
		void UpdateLightBuffers(gr::RenderDataManager const&); // Called once at the start of each frame


	public:
		// Get the monolithic light data buffer
		// NOTE: This buffer may be reallocated; It must be attached every frame as a single frame input ONLY
		re::BufferInput const& GetLightDataBuffer(gr::Light::Type) const;

		uint32_t GetLightDataBufferIdx(gr::Light::Type, gr::RenderDataID lightID) const;


	public:
		uint32_t GetShadowDataBufferIdx(gr::Light::Type, gr::RenderDataID lightID) const;


	public:
		std::shared_ptr<re::Texture> GetShadowArrayTexture(gr::Light::Type) const;

		// Shadow arrays may be reallocated at the beginning of any frame; Texture inputs should be reset each frame
		re::TextureView const& GetShadowArrayReadView(gr::Light::Type) const;

		uint32_t GetShadowArrayIndex(gr::Light::Type, gr::RenderDataID lightID) const;

		re::BufferInput const& GetPCSSPoissonSampleParamsBuffer() const;

	
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

			re::BufferInput m_lightData; // Always has at least 1 element (i.e. a dummy if no lights exist)
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

			re::TextureView m_readView;
		};
		ShadowMetadata m_directionalShadowMetadata;
		ShadowMetadata m_pointShadowMetadata;
		ShadowMetadata m_spotShadowMetadata;

		re::BufferInput m_poissonSampleParamsBuffer;


	private:
		// Get the logical array index (i.e. i * 6 = index of 2DArray face for a cubemap)
		uint32_t GetShadowArrayIndex(ShadowMetadata const&, gr::RenderDataID) const;


	private:
		void RemoveDeletedLights(gr::RenderDataManager const&);
		void RegisterNewLights(gr::RenderDataManager const& renderData);
		void UpdateLightBufferData(gr::RenderDataManager const& renderData);


	private: // No copying allowed
		LightManager(LightManager const&) = delete;
		LightManager& operator=(LightManager const&) = delete;
	};


	inline uint32_t LightManager::GetLightDataBufferIdx(gr::Light::Type lightType, gr::RenderDataID lightID) const
	{
		switch (lightType)
		{
		case gr::Light::Directional:
		{
			SEAssert(m_directionalLightMetadata.m_renderDataIDToBufferIdx.contains(lightID),
				"Light has not been registered");
			return m_directionalLightMetadata.m_renderDataIDToBufferIdx.at(lightID);
		}
		break;
		case gr::Light::Point:
		{
			SEAssert(m_pointLightMetadata.m_renderDataIDToBufferIdx.contains(lightID),
				"Light has not been registered");
			return m_pointLightMetadata.m_renderDataIDToBufferIdx.at(lightID);
		}
		break;
		case gr::Light::Spot:
		{
			SEAssert(m_spotLightMetadata.m_renderDataIDToBufferIdx.contains(lightID),
				"Light has not been registered");
			return m_spotLightMetadata.m_renderDataIDToBufferIdx.at(lightID);
		}
		break;
		case gr::Light::AmbientIBL:
		default: SEAssertF("Invalid light type");
		}
		return std::numeric_limits<uint32_t>::max(); // This should never happen
	}


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


	inline re::BufferInput const& LightManager::GetPCSSPoissonSampleParamsBuffer() const
	{
		return m_poissonSampleParamsBuffer;
	}
}