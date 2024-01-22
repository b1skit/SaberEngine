// © 2023 Adam Badke. All rights reserved.
#include "Batch.h"
#include "BatchManager.h"
#include "CastUtils.h"
#include "Material.h"
#include "MeshPrimitive.h"
#include "ProfilingMarkers.h"
#include "RenderDataManager.h"


namespace gr
{
	void BatchManager::UpdateBatchCache(gr::RenderDataManager const& renderData)
	{
		SEAssert(m_permanentCachedBatches.size() == m_renderDataIDToBatchMetadata.size() &&
			m_permanentCachedBatches.size() == m_cacheIdxToRenderDataID.size(),
			"Batch cache and batch maps are out of sync");

		// Remove deleted batches
		std::vector<gr::RenderDataID> deletedIDs = renderData.GetIDsWithDeletedData<gr::MeshPrimitive::RenderData>();
		for (gr::RenderDataID deletedID : deletedIDs)
		{
			if (gr::HasFeature(gr::RenderObjectFeature::IsMeshPrimitive, renderData.GetFeatureBits(deletedID)))
			{
				SEAssert(m_renderDataIDToBatchMetadata.contains(deletedID) && 
					m_cacheIdxToRenderDataID.contains(deletedID),
					"Failed to find batch metadata associated with the deleted ID");

				auto renderDataIDToMetadata = m_renderDataIDToBatchMetadata.find(deletedID);

				// Move the last batch to replace the one being deleted:
				const size_t cacheIdxToReplace = renderDataIDToMetadata->second.m_cacheIndex;
				const size_t cacheIdxToMove = m_permanentCachedBatches.size();
				if (cacheIdxToReplace != cacheIdxToMove)
				{
					m_permanentCachedBatches[cacheIdxToReplace] = re::Batch::Duplicate(
						m_permanentCachedBatches[cacheIdxToMove], 
						m_permanentCachedBatches[cacheIdxToMove].GetLifetime());
				}
				m_permanentCachedBatches.pop_back();

				// Update the metadata:
				const gr::RenderDataID movedRenderDataID = m_cacheIdxToRenderDataID.at(cacheIdxToMove);
				m_cacheIdxToRenderDataID.erase(cacheIdxToMove);
				m_cacheIdxToRenderDataID.emplace(cacheIdxToReplace, movedRenderDataID);

				m_renderDataIDToBatchMetadata.erase(deletedID);
				m_renderDataIDToBatchMetadata.at(movedRenderDataID).m_cacheIndex = cacheIdxToReplace;
			}
		}


		// Create batches for newly added IDs
		std::vector<gr::RenderDataID> newIDs = renderData.GetIDsWithNewData<gr::MeshPrimitive::RenderData>();
		auto newDataItr = renderData.IDBegin(newIDs);
		auto const& newDataItrEnd = renderData.IDEnd(newIDs);
		while (newDataItr != newDataItrEnd)
		{
			const gr::RenderDataID newDataID = newDataItr.GetRenderDataID();
			
			if (gr::HasFeature(gr::RenderObjectFeature::IsMeshPrimitive, newDataItr.GetFeatureBits()))
			{
				gr::MeshPrimitive::RenderData const& meshPrimRenderData = 
					newDataItr.Get<gr::MeshPrimitive::RenderData>();
				gr::Material::MaterialInstanceData const& materialRenderData = 
					newDataItr.Get<gr::Material::MaterialInstanceData>();

				const gr::TransformID newBatchTransformID = newDataItr.GetTransformID();
				const size_t newBatchIdx = m_permanentCachedBatches.size();

				m_permanentCachedBatches.emplace_back(
					re::Batch(re::Batch::Lifetime::Permanent, meshPrimRenderData, &materialRenderData));

				const uint64_t batchHash = m_permanentCachedBatches.back().GetDataHash();

				m_renderDataIDToBatchMetadata.emplace(
					newDataID,
					BatchMetadata{
						.m_batchHash = batchHash,
						.m_renderDataID = newDataID,
						.m_transformID = newBatchTransformID,
						.m_cacheIndex = newBatchIdx
					});
				m_cacheIdxToRenderDataID.emplace(newBatchIdx, newDataID);
			}

			++newDataItr;
		}
	}


	std::vector<re::Batch> BatchManager::BuildSceneBatches(
		gr::RenderDataManager const& renderData, 
		std::vector<gr::RenderDataID> const& renderDataIDs) const
	{
		std::vector<BatchMetadata> batchSortData;
		batchSortData.reserve(renderDataIDs.size());
		for (size_t i = 0; i < renderDataIDs.size(); i++)
		{
			SEAssert(m_renderDataIDToBatchMetadata.contains(renderDataIDs[i]), "Batch with the given ID does not exist");
			
			batchSortData.emplace_back(m_renderDataIDToBatchMetadata.at(renderDataIDs[i]));
		}

		// Assemble a list of instanced batches:
		std::vector<re::Batch> batches;
		batches.reserve(renderDataIDs.size());

		if (!batchSortData.empty())
		{
			// Sort the batch metadata:
			std::sort(batchSortData.begin(), batchSortData.end(),
				[](BatchMetadata const& a, BatchMetadata const& b) { return (a.m_batchHash > b.m_batchHash); });

			size_t unmergedIdx = 0;
			do
			{
				re::Batch const& cachedBatch = m_permanentCachedBatches[batchSortData[unmergedIdx].m_cacheIndex];

				// Add the first batch in the sequence to our final list. We duplicate the batch, as the cached batches
				// have a permanent Lifetime
				batches.emplace_back(re::Batch::Duplicate(cachedBatch, re::Batch::Lifetime::SingleFrame));

				const uint64_t curBatchHash = batchSortData[unmergedIdx].m_batchHash;

				// Find the index of the last batch with a matching hash in the sequence:
				const size_t instanceStartIdx = unmergedIdx++;
				while (unmergedIdx < batchSortData.size() &&
					batchSortData[unmergedIdx].m_batchHash == curBatchHash)
				{
					unmergedIdx++;
				}

				// Compute and set the number of instances in the batch:
				const uint32_t numInstances = util::CheckedCast<uint32_t, size_t>(unmergedIdx - instanceStartIdx);
				batches.back().SetInstanceCount(numInstances);

				// Gather the data we need to build our instanced parameter blocks:
				std::vector<gr::Transform::RenderData const*> instanceTransformRenderData;
				instanceTransformRenderData.reserve(numInstances);

				std::vector<gr::Material::MaterialInstanceData const*> instanceMaterialData;
				instanceMaterialData.reserve(numInstances);

				for (size_t instanceOffset = 0; instanceOffset < numInstances; instanceOffset++)
				{
					const size_t unmergedSrcIdx = instanceStartIdx + instanceOffset;

					gr::Transform::RenderData const* instanceTransformData =
						&renderData.GetTransformDataFromTransformID(batchSortData[unmergedSrcIdx].m_transformID);
					instanceTransformRenderData.emplace_back(instanceTransformData);

					gr::Material::MaterialInstanceData const* materialInstanceData = 
						&renderData.GetObjectData<gr::Material::MaterialInstanceData>(
							batchSortData[unmergedSrcIdx].m_renderDataID);
					instanceMaterialData.emplace_back(materialInstanceData);
				}

				// Finally, attach our instanced parameter blocks:
				batches.back().SetParameterBlock(gr::Transform::CreateInstancedTransformParams(
					instanceTransformRenderData));

				batches.back().SetParameterBlock(gr::Material::CreateInstancedParameterBlock(
					re::ParameterBlock::PBType::SingleFrame,
					instanceMaterialData));

			} while (unmergedIdx < batchSortData.size());
		}

		return batches;
	}
}