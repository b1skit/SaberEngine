// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "CameraRenderData.h"
#include "LightRenderData.h"
#include "RenderObjectIDs.h"

#include "Core/Util/HashKey.h"

namespace core
{
	template<typename T>
	class InvPtr;
}
namespace re
{
	class Batch;
	class VertexBufferInput;
}

namespace gr
{
	using TextureDependencies = std::map<util::HashKey, core::InvPtr<re::Texture> const*>;
	using BufferDependencies = std::map<util::HashKey, std::shared_ptr<re::Buffer> const*>;
	using DataDependencies = std::unordered_map<util::HashKey, void const*>;


	// Data inputs/output types:
	using ViewCullingResults = std::map<gr::Camera::View const, std::vector<gr::RenderDataID>>;
	using PunctualLightCullingResults = std::vector<gr::RenderDataID>;

	using AnimatedVertexStreams = std::unordered_map<
		gr::RenderDataID, std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams>>;

	using ViewBatches = std::unordered_map<gr::Camera::View const, std::vector<re::Batch>>;
	using AllBatches = std::vector<re::Batch>;

	using LightDataBufferIdxMap = std::unordered_map<gr::RenderDataID, uint32_t>;
	
	using ShadowArrayIdxMap = std::unordered_map<gr::RenderDataID, uint32_t>;
	static constexpr uint32_t k_invalidShadowIndex = std::numeric_limits<uint32_t>::max();


	// Helper functions:
	uint32_t GetLightDataBufferIdx(LightDataBufferIdxMap const*, gr::RenderDataID lightID);
	uint32_t GetShadowArrayIdx(ShadowArrayIdxMap const*, gr::RenderDataID lightID);


	template<typename T>
	T const* GetDataDependency(util::HashKey const& scriptName, DataDependencies const& dataDependencies)
	{
		auto const& result = dataDependencies.find(scriptName);
		return result == dataDependencies.end() ? nullptr : static_cast<T const*>(result->second);
	}
}