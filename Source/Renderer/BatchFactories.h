// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Batch.h"
#include "Effect.h"
#include "RenderObjectIDs.h"

#include "Core/InvPtr.h"


namespace gr
{
	class MeshPrimitive;
	class RasterBatchBuilder;
	class RenderDataManager;
}

namespace grutil
{
	extern gr::RasterBatchBuilder&& BuildInstancedRasterBatch(
		gr::RasterBatchBuilder&& batchBuilder,
		re::Batch::VertexStreamOverride const* vertexStreamOverrides,
		gr::RenderDataID renderDataID,
		gr::RenderDataManager const& renderData);

	extern gr::RasterBatchBuilder&& BuildMeshPrimitiveRasterBatch(
		gr::RasterBatchBuilder&& batchBuilder,
		core::InvPtr<gr::MeshPrimitive> const&,
		EffectID);
}