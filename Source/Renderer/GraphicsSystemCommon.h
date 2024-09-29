// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "CameraRenderData.h"
#include "LightRenderData.h"
#include "RenderObjectIDs.h"

#include "Core/Util/HashKey.h"


namespace gr
{
	using TextureDependencies = std::map<util::HashKey const, std::shared_ptr<re::Texture> const*>;
	using BufferDependencies = std::map<util::HashKey const, std::shared_ptr<re::Buffer> const*>;
	using DataDependencies = std::unordered_map<util::HashKey const, void const*>;


	// Data inputs/output types:
	using ViewCullingResults = std::unordered_map<gr::Camera::View const, std::vector<gr::RenderDataID>>;
	using PunctualLightCullingResults = std::vector<gr::RenderDataID>;

	using LightDataBufferIdxMap = std::unordered_map<gr::RenderDataID, uint32_t>;


	// Helper functions:
	uint32_t GetLightDataBufferIdx(LightDataBufferIdxMap const&, gr::RenderDataID lightID);


	template<typename T>
	T const* GetDataDependency(util::HashKey const& scriptName, DataDependencies const& dataDependencies)
	{
		auto const& result = dataDependencies.find(scriptName);
		return result == dataDependencies.end() ? nullptr : static_cast<T const*>(result->second);
	}
}