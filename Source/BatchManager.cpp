// © 2023 Adam Badke. All rights reserved.
#include "Batch.h"
#include "BatchManager.h"
#include "MeshPrimitive.h"
#include "ProfilingMarkers.h"
#include "RenderDataManager.h"

#include "Core\Util\CastUtils.h"
#include "Core\Util\MathUtils.h"

#include "Shaders/Common/InstancingParams.h"
#include "Shaders/Common/MaterialParams.h"


namespace
{
	// We'll round our buffer array sizes up to the nearest multiple of this value
	constexpr uint32_t k_numBlocksPerAllocation = 64;


	InstanceIndices CreateInstanceIndicesEntry(uint32_t transformIdx, uint32_t materialIdx)
	{
		return InstanceIndices
		{
			.g_transformIdx = transformIdx,
			.g_materialIdx = materialIdx,
			
			._padding = glm::uvec2(0)
		};
	}


	std::shared_ptr<re::Buffer> CreateInstanceIndexBuffer(
		re::Buffer::Type bufferType,
		std::vector<InstanceIndices> const& instanceIndices)
	{
		SEAssert(instanceIndices.size() < InstanceIndexData::k_maxInstances,
			"Too many instances, consider increasing INSTANCE_ARRAY_SIZE/k_maxInstances");

		InstanceIndexData instanceIndexBufferData{};

		memcpy(&instanceIndexBufferData.g_instanceIndices,
			instanceIndices.data(), 
			sizeof(InstanceIndices) * instanceIndices.size());

		return re::Buffer::Create(
			InstanceIndexData::s_shaderName,
			instanceIndexBufferData,
			bufferType);
	}


	template<typename T>
	void AssignInstancingIndex(
		std::unordered_map<T, gr::BatchManager::RefCountedIndex>& indexMap,
		std::vector<uint32_t>& freeIndexes, 
		T newID)
	{
		// Transforms can be shared; We only need a single index in our array
		if (indexMap.contains(newID))
		{
			gr::BatchManager::RefCountedIndex& refCountedIndex = indexMap.at(newID);
			refCountedIndex.m_refCount++;
		}
		else
		{
			uint32_t newIndex = std::numeric_limits<uint32_t>::max();
			if (!freeIndexes.empty())
			{
				// If an index has been returned, reuse it:
				newIndex = freeIndexes.back();
				freeIndexes.pop_back();
			}
			else
			{
				// No recycled indexes exist; Assign a new, monotonically-increasing index:
				newIndex = util::CheckedCast<uint32_t>(indexMap.size());
			}

			indexMap.emplace(newID,
				gr::BatchManager::RefCountedIndex{
					.m_index = newIndex,
					.m_refCount = 1
				});
		}
	}


	template<typename T>
	void FreeInstancingIndex(
		std::unordered_map<T, gr::BatchManager::RefCountedIndex>& indexMap,
		std::vector<uint32_t>& freeIndexes,
		T idToFree)
	{
		SEAssert(indexMap.contains(idToFree), "ID has not been assigned an index");

		gr::BatchManager::RefCountedIndex& refCountedIndex = indexMap.at(idToFree);
		SEAssert(refCountedIndex.m_refCount >= 1, "Invalid ref count");

		refCountedIndex.m_refCount--;

		if (indexMap.at(idToFree).m_refCount == 0)
		{
			freeIndexes.emplace_back(refCountedIndex.m_index);
			indexMap.erase(idToFree);
		}
	}
}

namespace gr
{
	BatchManager::BatchManager()
	{
		m_instancedTransformIndexes.reserve(k_numBlocksPerAllocation);
		m_freeTransformIndexes.reserve(k_numBlocksPerAllocation);

		for (auto& matInstData : m_materialInstanceMetadata)
		{
			matInstData.m_instancedMaterialIndexes.reserve(k_numBlocksPerAllocation);
			matInstData.m_freeInstancedMaterialIndexes.reserve(k_numBlocksPerAllocation);
		}
	}


	void BatchManager::UpdateBatchCache(gr::RenderDataManager const& renderData)
	{
		SEAssert(m_permanentCachedBatches.size() == m_renderDataIDToBatchMetadata.size() &&
			m_permanentCachedBatches.size() == m_cacheIdxToRenderDataID.size(),
			"Batch cache and batch maps are out of sync");

		// Remove deleted batches
		std::vector<gr::RenderDataID> const& deletedMeshPrimIDs = 
			renderData.GetIDsWithDeletedData<gr::MeshPrimitive::RenderData>();
		for (gr::RenderDataID renderDataIDToDelete : deletedMeshPrimIDs)
		{
			if (m_renderDataIDToBatchMetadata.contains(renderDataIDToDelete))
			{
				auto deletedIDMetadataItr = m_renderDataIDToBatchMetadata.find(renderDataIDToDelete);

				const gr::TransformID deletedTransformID = deletedIDMetadataItr->second.m_transformID;

				// Move the last batch to replace the one being deleted:
				const size_t cacheIdxToReplace = deletedIDMetadataItr->second.m_cacheIndex;

				SEAssert(!m_permanentCachedBatches.empty() && cacheIdxToReplace < m_permanentCachedBatches.size(),
					"Permanent cached batches cannot be empty, and the index being replaced must be in bounds");

				const size_t cacheIdxToMove = m_permanentCachedBatches.size() - 1;

				const gr::Material::MaterialEffect matEffect = 
					m_renderDataIDToBatchMetadata.at(renderDataIDToDelete).m_matEffect;

				SEAssert(m_cacheIdxToRenderDataID.contains(cacheIdxToMove), "Cache index not found");
				const gr::RenderDataID renderDataIDToMove = m_cacheIdxToRenderDataID.at(cacheIdxToMove);

				SEAssert(m_cacheIdxToRenderDataID.at(cacheIdxToReplace) == renderDataIDToDelete,
					"Cache index to ID map references a different ID");

				m_cacheIdxToRenderDataID.erase(cacheIdxToMove);
				m_renderDataIDToBatchMetadata.erase(renderDataIDToDelete);

				if (cacheIdxToReplace != cacheIdxToMove)
				{
					m_permanentCachedBatches[cacheIdxToReplace] = re::Batch::Duplicate(
						m_permanentCachedBatches[cacheIdxToMove], 
						m_permanentCachedBatches[cacheIdxToMove].GetLifetime());

					SEAssert(m_cacheIdxToRenderDataID.contains(cacheIdxToReplace), "Cache index not found");
					
					SEAssert(m_renderDataIDToBatchMetadata.contains(renderDataIDToMove),
						"Cannot find the render data ID to move");

					SEAssert(m_renderDataIDToBatchMetadata.at(renderDataIDToMove).m_renderDataID == renderDataIDToMove,
						"IDs are out of sync");

					m_cacheIdxToRenderDataID.at(cacheIdxToReplace) = renderDataIDToMove;
					m_renderDataIDToBatchMetadata.at(renderDataIDToMove).m_cacheIndex = cacheIdxToReplace;
				}
				m_permanentCachedBatches.pop_back();


				// Update the metadata:
				FreeInstancingIndex(m_instancedTransformIndexes, m_freeTransformIndexes, deletedTransformID);
				
				FreeInstancingIndex(
					m_materialInstanceMetadata[matEffect].m_instancedMaterialIndexes, 
					m_materialInstanceMetadata[matEffect].m_freeInstancedMaterialIndexes, 
					renderDataIDToDelete);
			}
		}


		// Create batches for newly added IDs
		std::vector<gr::RenderDataID> const& newMeshPrimIDs = 
			renderData.GetIDsWithNewData<gr::MeshPrimitive::RenderData>();
		auto newMeshPrimDataItr = renderData.IDBegin(newMeshPrimIDs);
		auto const& newMeshPrimDataItrEnd = renderData.IDEnd(newMeshPrimIDs);
		while (newMeshPrimDataItr != newMeshPrimDataItrEnd)
		{
			const gr::RenderDataID newMeshPrimID = newMeshPrimDataItr.GetRenderDataID();
			
			if (gr::HasFeature(gr::RenderObjectFeature::IsMeshPrimitive, newMeshPrimDataItr.GetFeatureBits()))
			{
				gr::MeshPrimitive::RenderData const& meshPrimRenderData = 
					newMeshPrimDataItr.Get<gr::MeshPrimitive::RenderData>();
				gr::Material::MaterialInstanceData const& materialRenderData = 
					newMeshPrimDataItr.Get<gr::Material::MaterialInstanceData>();

				const gr::TransformID newBatchTransformID = newMeshPrimDataItr.GetTransformID();
				const size_t newBatchIdx = m_permanentCachedBatches.size();

				m_permanentCachedBatches.emplace_back(
					re::Batch(re::Batch::Lifetime::Permanent, meshPrimRenderData, &materialRenderData));

				const uint64_t batchHash = m_permanentCachedBatches.back().GetDataHash();

				// Update the metadata:
				m_cacheIdxToRenderDataID.emplace(newBatchIdx, newMeshPrimID);

				const gr::Material::MaterialEffect matEffect = materialRenderData.m_matEffect;

				m_renderDataIDToBatchMetadata.emplace(
					newMeshPrimID,
					BatchMetadata{
						.m_batchHash = batchHash,
						.m_renderDataID = newMeshPrimID,
						.m_transformID = newBatchTransformID,
						.m_matEffect = matEffect,
						.m_cacheIndex = newBatchIdx
					});

				AssignInstancingIndex(
					m_instancedTransformIndexes,
					m_freeTransformIndexes,
					newBatchTransformID);

				AssignInstancingIndex(
					m_materialInstanceMetadata[matEffect].m_instancedMaterialIndexes,
					m_materialInstanceMetadata[matEffect].m_freeInstancedMaterialIndexes,
					newMeshPrimID);
			}

			++newMeshPrimDataItr;
		}

		
		// Create/grow our permanent Transform instance buffers:
		const bool mustReallocateTransformBuffer = m_instancedTransforms != nullptr && 
			m_instancedTransforms->GetNumElements() < m_instancedTransformIndexes.size();

		const uint32_t requestedTransformBufferElements = util::RoundUpToNearestMultiple(
			util::CheckedCast<uint32_t>(m_instancedTransformIndexes.size()),
			k_numBlocksPerAllocation);

		if ((mustReallocateTransformBuffer || m_instancedTransforms == nullptr) && requestedTransformBufferElements > 0)
		{
			m_instancedTransforms = re::Buffer::CreateUncommittedArray<InstancedTransformData>(
				InstancedTransformData::s_shaderName,
				requestedTransformBufferElements,
				re::Buffer::Type::Mutable);

			// If we reallocated, re-copy all of the data to the new buffer
			if (mustReallocateTransformBuffer)
			{
				LOG_WARNING("gr::BatchManager: Transform instance buffer is being reallocated");

				for (auto& transformRecord : m_instancedTransformIndexes)
				{
					SEAssert(transformRecord.second.m_refCount >= 1, "Invalid ref count");

					gr::TransformID transformID = transformRecord.first;
					const uint32_t transformIdx = transformRecord.second.m_index;

					gr::Transform::RenderData const& transformData = 
						renderData.GetTransformDataFromTransformID(transformID);

					InstancedTransformData const& transformParams =
						gr::Transform::CreateInstancedTransformData(transformData);

					m_instancedTransforms->Commit(
						&transformParams,
						transformIdx,
						1);
				}
			}
		}

		// Create/grow our permanent Material instance buffers:
		for (uint32_t matEffectIdx = 0; matEffectIdx < gr::Material::MaterialEffect_Count; matEffectIdx++)
		{
			MaterialInstanceMetadata& matInstMeta = m_materialInstanceMetadata[matEffectIdx];

			const bool mustReallocateMaterialBuffer = 
				matInstMeta.m_instancedMaterials != nullptr &&
				matInstMeta.m_instancedMaterials->GetNumElements() < matInstMeta.m_instancedMaterialIndexes.size();

			const uint32_t requestedMaterialBufferElements = util::RoundUpToNearestMultiple(
				util::CheckedCast<uint32_t>(matInstMeta.m_instancedMaterialIndexes.size()),
				k_numBlocksPerAllocation);

			if ((mustReallocateMaterialBuffer || matInstMeta.m_instancedMaterials == nullptr) && 
				requestedMaterialBufferElements > 0)
			{
				matInstMeta.m_instancedMaterials = gr::Material::ReserveInstancedBuffer(
					static_cast<gr::Material::MaterialEffect>(matEffectIdx), requestedMaterialBufferElements);

				if (mustReallocateMaterialBuffer)
				{
					LOG_WARNING("gr::BatchManager: Material instance buffer \"%s\"is being reallocated",
						gr::Material::k_materialEffectNames[matEffectIdx]);

					for (auto& materialRecord : matInstMeta.m_instancedMaterialIndexes)
					{
						SEAssert(materialRecord.second.m_refCount >= 1, "Invalid ref count");

						const gr::RenderDataID materialID = materialRecord.first;
						const uint32_t materialIdx = materialRecord.second.m_index;

						gr::Material::MaterialInstanceData const& materialData =
							renderData.GetObjectData<gr::Material::MaterialInstanceData>(materialID);

						gr::Material::CommitMaterialInstanceData(
							matInstMeta.m_instancedMaterials.get(),
							&materialData,
							materialIdx);
					}
				}
			}
		}

		// Update dirty instanced Transform data:
		std::vector<gr::TransformID> const& dirtyTransforms = renderData.GetIDsWithDirtyTransformData();
		for (gr::TransformID transformID : dirtyTransforms)
		{
			// Lots of things have a Transform; We only care about Transforms we found while parsing things we're
			// instancing (e.g. MeshPrimitives)
			if (m_instancedTransformIndexes.contains(transformID))
			{
				const uint32_t transformIdx = m_instancedTransformIndexes.at(transformID).m_index;

				gr::Transform::RenderData const& transformData = renderData.GetTransformDataFromTransformID(transformID);

				InstancedTransformData const& transformParams = 
					gr::Transform::CreateInstancedTransformData(transformData);

				m_instancedTransforms->Commit(
					&transformParams,
					transformIdx,
					1);
			}
		}
		
		// Update dirty instanced Material data:
		if (renderData.HasObjectData<gr::Material::MaterialInstanceData>())
		{
			std::vector<gr::RenderDataID> const& dirtyMaterials =
				renderData.GetIDsWithDirtyData<gr::Material::MaterialInstanceData>();

			auto dirtyMaterialItr = renderData.IDBegin(dirtyMaterials);
			auto const& dirtyMaterialItrEnd = renderData.IDEnd(dirtyMaterials);
			while (dirtyMaterialItr != dirtyMaterialItrEnd)
			{
				const gr::RenderDataID dirtyMaterialID = dirtyMaterialItr.GetRenderDataID();

				if (gr::HasFeature(gr::RenderObjectFeature::IsMeshPrimitive, dirtyMaterialItr.GetFeatureBits()))
				{
					MaterialInstanceMetadata& matInstMeta = 
						m_materialInstanceMetadata[m_renderDataIDToBatchMetadata.at(dirtyMaterialID).m_matEffect];

					SEAssert(matInstMeta.m_instancedMaterialIndexes.contains(dirtyMaterialID),
						"RenderDataID has not been registered for instancing indexes");

					const uint32_t materialIdx = matInstMeta.m_instancedMaterialIndexes.at(dirtyMaterialID).m_index;

					gr::Material::MaterialInstanceData const& materialData =
						renderData.GetObjectData<gr::Material::MaterialInstanceData>(dirtyMaterialID);

					gr::Material::CommitMaterialInstanceData(
						matInstMeta.m_instancedMaterials.get(),
						&materialData,
						materialIdx);

					// Recreate the associated Batch in case something on the Material that modifies the Batch behavior
					// has changed (e.g. filter bits: shadow casting enabled/disabled)
					re::Batch& permanentBatch = 
						m_permanentCachedBatches.at(m_renderDataIDToBatchMetadata.at(dirtyMaterialID).m_cacheIndex);
					permanentBatch = re::Batch(
						re::Batch::Lifetime::Permanent, 
						renderData.GetObjectData<gr::MeshPrimitive::RenderData>(dirtyMaterialID),
						&materialData);
				}
				++dirtyMaterialItr;
			}
		}
	}


	std::vector<re::Batch> BatchManager::GetSceneBatches(
		std::vector<gr::RenderDataID> const& renderDataIDs,
		uint8_t bufferTypeMask /*= (InstanceType::Transform | InstanceType::Material)*/,
		re::Batch::FilterBitmask required /*=0*/,
		re::Batch::FilterBitmask excluded /*= 0*/) const
	{
		// Copy the batch metadata for the requeted RenderDataIDs:
		std::vector<BatchMetadata> batchMetadata;
		batchMetadata.reserve(renderDataIDs.size());
		for (size_t i = 0; i < renderDataIDs.size(); i++)
		{
			SEAssert(m_renderDataIDToBatchMetadata.contains(renderDataIDs[i]),
				"Batch with the given ID does not exist");
			
			batchMetadata.emplace_back(m_renderDataIDToBatchMetadata.at(renderDataIDs[i]));
		}

		// Assemble a list of instanced batches:
		std::vector<re::Batch> batches;
		batches.reserve(batchMetadata.size());

		if (!batchMetadata.empty())
		{
			// Sort the batch metadata:
			std::sort(batchMetadata.begin(), batchMetadata.end(),
				[](BatchMetadata const& a, BatchMetadata const& b) { return (a.m_batchHash < b.m_batchHash); });

			size_t unmergedIdx = 0;
			do
			{
				re::Batch const& cachedBatch = m_permanentCachedBatches[batchMetadata[unmergedIdx].m_cacheIndex];

				// Pre-filter the batch. RenderStages will also filter batches, but this allows us to minimize
				// unnecessary copying when we know certain batches aren't required
				if (!cachedBatch.MatchesFilterBits(required, excluded))
				{
					unmergedIdx++; // Skip the batch
					continue;
				}

				// Add the first batch in the sequence to our final list. We duplicate the batch, as the cached batches
				// have a permanent Lifetime
				batches.emplace_back(re::Batch::Duplicate(cachedBatch, re::Batch::Lifetime::SingleFrame));

				const uint64_t curBatchHash = batchMetadata[unmergedIdx].m_batchHash;

				// Obtain the Material instance metadata while we still have the current unmergedIdx		
				MaterialInstanceMetadata const& matInstMeta =
					m_materialInstanceMetadata[batchMetadata[unmergedIdx].m_matEffect];

				// Find the index of the last batch with a matching hash in the sequence:
				const size_t instanceStartIdx = unmergedIdx++;
				while (unmergedIdx < batchMetadata.size() &&
					batchMetadata[unmergedIdx].m_batchHash == curBatchHash)
				{
					unmergedIdx++;
				}

				// Compute and set the number of instances in the batch:
				const uint32_t numInstances = util::CheckedCast<uint32_t, size_t>(unmergedIdx - instanceStartIdx);
				batches.back().SetInstanceCount(numInstances);

				// Gather the data we need to build our instanced buffers:
				std::vector<InstanceIndices> instanceIndices;
				instanceIndices.reserve(numInstances);

				for (size_t instanceOffset = 0; instanceOffset < numInstances; instanceOffset++)
				{
					const size_t unmergedSrcIdx = instanceStartIdx + instanceOffset;

					SEAssert(m_instancedTransformIndexes.contains(batchMetadata[unmergedSrcIdx].m_transformID),
						"TransformID is not registered for an instanced transform index");
					SEAssert(matInstMeta.m_instancedMaterialIndexes.contains(batchMetadata[unmergedSrcIdx].m_renderDataID),
						"RenderDataID is not registered for an instanced material index");

					const uint32_t transformIdx =
						m_instancedTransformIndexes.at(batchMetadata[unmergedSrcIdx].m_transformID).m_index;

					const uint32_t materialIdx =
						matInstMeta.m_instancedMaterialIndexes.at(batchMetadata[unmergedSrcIdx].m_renderDataID).m_index;

					instanceIndices.emplace_back(CreateInstanceIndicesEntry(transformIdx, materialIdx));
				}

				// Finally, attach our instanced buffers:
				if (bufferTypeMask != 0)
				{
					batches.back().SetBuffer(CreateInstanceIndexBuffer(re::Buffer::Type::SingleFrame, instanceIndices));

					if (bufferTypeMask & InstanceType::Transform)
					{
						batches.back().SetBuffer(m_instancedTransforms);
					}
					if (bufferTypeMask & InstanceType::Material)
					{
						batches.back().SetBuffer(matInstMeta.m_instancedMaterials);
					}
				}

			} while (unmergedIdx < batchMetadata.size());
		}

		return batches;
	}


	std::vector<re::Batch> BatchManager::GetAllSceneBatches(
		uint8_t bufferTypeMask /*= (InstanceType::Transform | InstanceType::Material)*/,
		re::Batch::FilterBitmask required /*=0*/,
		re::Batch::FilterBitmask excluded /*= 0*/) const
	{
		std::vector<gr::RenderDataID> allRenderDataIDs;
		allRenderDataIDs.reserve(m_renderDataIDToBatchMetadata.size());

		for (auto const& metadata : m_renderDataIDToBatchMetadata)
		{
			allRenderDataIDs.emplace_back(metadata.first);
		}

		return GetSceneBatches(allRenderDataIDs, bufferTypeMask, required, excluded);
	}
}