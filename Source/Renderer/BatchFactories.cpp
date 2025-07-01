// © 2025 Adam Badke. All rights reserved.
#include "BatchBuilder.h"
#include "BatchFactories.h"
#include "Material.h"
#include "RenderDataManager.h"

#include "Core/Assert.h"


namespace grutil
{
	gr::RasterBatchBuilder&& BuildInstancedRasterBatch(
		gr::RasterBatchBuilder&& batchBuilder,
		re::Batch::VertexStreamOverride const* vertexStreamOverrides,
		gr::RenderDataID renderDataID,
		gr::RenderDataManager const& renderData)
	{
		SEAssert(renderData.HasObjectData<gr::MeshPrimitive::RenderData>(),
			"This callback requires MeshPrimitive RenderData");

		gr::MeshPrimitive::RenderData const& meshPrimRenderData =
			renderData.GetObjectData<gr::MeshPrimitive::RenderData>(renderDataID);

		std::move(batchBuilder).SetGeometryMode(re::Batch::GeometryMode::IndexedInstanced);
		std::move(batchBuilder).SetPrimitiveTopology(meshPrimRenderData.m_meshPrimitiveParams.m_primitiveTopology);

		if (vertexStreamOverrides)
		{
			std::move(batchBuilder).SetVertexStreamOverrides(vertexStreamOverrides);
		}

		// We assume the MeshPrimitive's vertex streams are ordered such that identical stream types are tightly
		// packed, and in the correct channel order corresponding to the final shader slots (e.g. uv0, uv1, etc)
		for (uint8_t slotIdx = 0; slotIdx < static_cast<uint8_t>(meshPrimRenderData.m_numVertexStreams); slotIdx++)
		{
			if (meshPrimRenderData.m_vertexStreams[slotIdx] == nullptr)
			{
				break;
			}

			std::move(batchBuilder).SetVertexBuffer(
				slotIdx,
				re::VertexBufferInput(meshPrimRenderData.m_vertexStreams[slotIdx]));
		}
		std::move(batchBuilder).SetIndexBuffer(meshPrimRenderData.m_indexStream);

		SEAssert(meshPrimRenderData.m_indexStream != nullptr,
			"This callback is for IndexedInstanced geometry. The index buffer cannot be null");

		// Material textures/samplers:
		if (renderData.HasObjectData<gr::Material::MaterialInstanceRenderData>(renderDataID))
		{
			gr::Material::MaterialInstanceRenderData const& materialInstanceData =
				renderData.GetObjectData<gr::Material::MaterialInstanceRenderData>(renderDataID);

			std::move(batchBuilder).SetEffectID(materialInstanceData.m_effectID);

			for (size_t i = 0; i < materialInstanceData.m_textures.size(); i++)
			{
				if (materialInstanceData.m_textures[i] && materialInstanceData.m_samplers[i])
				{
					std::move(batchBuilder).SetTextureInput(
						materialInstanceData.m_shaderSamplerNames[i],
						materialInstanceData.m_textures[i],
						materialInstanceData.m_samplers[i],
						re::TextureView(materialInstanceData.m_textures[i]));
				}
			}

			std::move(batchBuilder).SetMaterialUniqueID(materialInstanceData.m_srcMaterialUniqueID);

			// Filter bits:
			std::move(batchBuilder).SetFilterMaskBit(
				re::Batch::Filter::AlphaBlended, materialInstanceData.m_alphaMode == gr::Material::AlphaMode::Blend);

			std::move(batchBuilder).SetFilterMaskBit(
				re::Batch::Filter::ShadowCaster, materialInstanceData.m_isShadowCaster);

			std::move(batchBuilder).SetDrawstyleBitmask(
				gr::Material::MaterialInstanceRenderData::GetDrawstyleBits(&materialInstanceData));
		}

		return std::move(batchBuilder);
	}


	gr::RasterBatchBuilder&& BuildMeshPrimitiveRasterBatch(
		gr::RasterBatchBuilder&& batchBuilder,
		core::InvPtr<gr::MeshPrimitive> const& meshPrim,
		EffectID effectID)
	{
		SEAssert(meshPrim->GetIndexStream() != nullptr,
			"This constructor is for IndexedInstanced geometry. The index buffer cannot be null");

		std::move(batchBuilder).SetGeometryMode(re::Batch::GeometryMode::IndexedInstanced);
		std::move(batchBuilder).SetPrimitiveTopology(meshPrim->GetMeshParams().m_primitiveTopology);
		std::move(batchBuilder).SetEffectID(effectID);

		// We assume the meshPrimitive's vertex streams are ordered such that identical stream types are tightly
		// packed, and in the correct channel order corresponding to the final shader slots (e.g. uv0, uv1, etc)
		std::vector<gr::MeshPrimitive::MeshVertexStream> const& vertexStreams = meshPrim->GetVertexStreams();
		for (uint8_t slotIdx = 0; slotIdx < static_cast<uint8_t>(vertexStreams.size()); slotIdx++)
		{
			if (vertexStreams[slotIdx].m_vertexStream == nullptr)
			{
				break;
			}
			std::move(batchBuilder).SetVertexBuffer(slotIdx, re::VertexBufferInput(vertexStreams[slotIdx].m_vertexStream));
		}
		std::move(batchBuilder).SetIndexBuffer(meshPrim->GetIndexStream());

		return std::move(batchBuilder);
	}
}