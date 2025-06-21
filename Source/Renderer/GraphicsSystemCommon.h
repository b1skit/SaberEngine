// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "CameraRenderData.h"
#include "VertexStream.h"
#include "RenderObjectIDs.h"

#include "Core/InvPtr.h"

#include "Core/Util/CHashKey.h"

namespace gr
{
	class BatchHandle;
}
namespace re
{
	class AccelerationStructure;
	class Texture;
	class VertexBufferInput;
}

namespace gr
{
	using TextureDependencies = std::map<util::CHashKey, core::InvPtr<re::Texture> const*>;
	using BufferDependencies = std::map<util::CHashKey, std::shared_ptr<re::Buffer> const*>;
	using DataDependencies = std::unordered_map<util::CHashKey, void const*>;


	// Data inputs/output types:
	using ViewCullingResults = std::map<gr::Camera::View const, std::vector<gr::RenderDataID>>;
	using PunctualLightCullingResults = std::vector<gr::RenderDataID>;

	using AnimatedVertexStreams = std::unordered_map<
		gr::RenderDataID, std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams>>;

	using TLAS = std::shared_ptr<re::AccelerationStructure>;

	using ViewBatches = std::unordered_map<gr::Camera::View const, std::vector<gr::BatchHandle>>;
	using AllBatches = std::vector<gr::BatchHandle>;

	struct ShadowRecord
	{
		core::InvPtr<re::Texture> const* m_shadowTex;
		uint32_t m_shadowTexArrayIdx;		
	};
	using LightIDToShadowRecordMap = std::unordered_map<gr::RenderDataID, gr::ShadowRecord>;

	// Helpers:
	template<typename T>
	T const* GetDataDependency(util::CHashKey const& scriptName, DataDependencies const& dataDependencies)
	{
		auto const& result = dataDependencies.find(scriptName);
		return result == dataDependencies.end() ? nullptr : static_cast<T const*>(result->second);
	}
}