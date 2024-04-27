// © 2023 Adam Badke. All rights reserved.
#include "Batch.h"
#include "BatchManager.h"
#include "Core\Util\CastUtils.h"
#include "Material_GLTF.h"
#include "Core\Util\MathUtils.h"
#include "MeshPrimitive.h"
#include "ProfilingMarkers.h"
#include "RenderDataManager.h"

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


	template<typename T>
	std::shared_ptr<re::Buffer> CreateInstancedBuffer(char const* shaderName, uint32_t maxInstances)
	{
		return re::Buffer::CreateUncommittedArray<T>(
			shaderName,
			maxInstances,
			re::Buffer::Type::Mutable);
	}
}

namespace gr
{
	BatchManager::BatchManager()
	{
		m_instancedTransformIndexes.reserve(k_numBlocksPerAllocation);
		m_freeTransformIndexes.reserve(k_numBlocksPerAllocation);

		m_instancedMaterialIndexes.reserve(k_numBlocksPerAllocation);
		m_freeInstancedMaterialIndexes.reserve(k_numBlocksPerAllocation);
	}


	void BatchManager::UpdateBatchCache(gr::RenderDataManager const& renderData)
	{
		SEAssert(m_permanentCachedBatches.size() == m_renderDataIDToBatchMetadata.size() &&
			m_permanentCachedBatches.size() == m_cacheIdxToRenderDataID.size(),
			"Batch cache and batch maps are out of sync");

		// Remove deleted batches
		std::vector<gr::RenderDataID> const& deletedIDs = renderData.GetIDsWithDeletedData<gr::MeshPrimitive::RenderData>();
		for (gr::RenderDataID idToDelete : deletedIDs)
		{
			if (m_renderDataIDToBatchMetadata.contains(idToDelete))
			{
				auto deletedIDMetadataItr = m_renderDataIDToBatchMetadata.find(idToDelete);

				const gr::TransformID deletedTransformID = deletedIDMetadataItr->second.m_transformID;

				// Move the last batch to replace the one being deleted:
				const size_t cacheIdxToReplace = deletedIDMetadataItr->second.m_cacheIndex;

				SEAssert(!m_permanentCachedBatches.empty() && cacheIdxToReplace < m_permanentCachedBatches.size(),
					"Permanent cached batches cannot be empty, and the index being replaced must be in bounds");

				const size_t cacheIdxToMove = m_permanentCachedBatches.size() - 1;

				SEAssert(m_cacheIdxToRenderDataID.contains(cacheIdxToMove), "Cache index not found");
				const gr::RenderDataID renderDataIDToMove = m_cacheIdxToRenderDataID.at(cacheIdxToMove);

				SEAssert(m_cacheIdxToRenderDataID.at(cacheIdxToReplace) == idToDelete,
					"Cache index to ID map references a different ID");

				m_cacheIdxToRenderDataID.erase(cacheIdxToMove);
				m_renderDataIDToBatchMetadata.erase(idToDelete);

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
				FreeInstancingIndex(m_instancedMaterialIndexes, m_freeInstancedMaterialIndexes, idToDelete);
			}
		}


		// Create batches for newly added IDs
		std::vector<gr::RenderDataID> const& newIDs = renderData.GetIDsWithNewData<gr::MeshPrimitive::RenderData>();
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

				// Update the metadata:
				m_cacheIdxToRenderDataID.emplace(newBatchIdx, newDataID);

				m_renderDataIDToBatchMetadata.emplace(
					newDataID,
					BatchMetadata{
						.m_batchHash = batchHash,
						.m_renderDataID = newDataID,
						.m_transformID = newBatchTransformID,
						.m_cacheIndex = newBatchIdx
					});

				AssignInstancingIndex(
					m_instancedTransformIndexes,
					m_freeTransformIndexes,
					newBatchTransformID);

				AssignInstancingIndex(
					m_instancedMaterialIndexes, 
					m_freeInstancedMaterialIndexes, 
					newDataID);
			}

			++newDataItr;
		}

		
		// Create/grow our permanent instance buffers:
		const bool mustReallocateTransformBuffer = m_instancedTransforms != nullptr && 
			m_instancedTransforms->GetNumElements() < m_instancedTransformIndexes.size();

		const uint32_t requestedTransformBufferElements = util::RoundUpToNearestMultiple(
			util::CheckedCast<uint32_t>(m_instancedTransformIndexes.size()),
			k_numBlocksPerAllocation);

		if ((mustReallocateTransformBuffer || m_instancedTransforms == nullptr) && requestedTransformBufferElements > 0)
		{
			m_instancedTransforms = CreateInstancedBuffer<InstancedTransformData>(
				InstancedTransformData::s_shaderName,
				requestedTransformBufferElements);

			// If we reallocated, re-copy all of the data to the new buffer
			if (mustReallocateTransformBuffer)
			{
				LOG_WARNING("gr::BatchManager: Transform instance buffer is being reallocated");

				for (auto& transformRecord : m_instancedTransformIndexes)
				{
					SEAssert(transformRecord.second.m_refCount >= 1, "Invalid ref count");

					gr::TransformID transformID = transformRecord.first;
					const uint32_t transformIdx = transformRecord.second.m_index;

					gr::Transform::RenderData const& transformData = renderData.GetTransformDataFromTransformID(transformID);

					InstancedTransformData const& transformParams =
						gr::Transform::CreateInstancedTransformData(transformData);

					m_instancedTransforms->Commit(
						&transformParams,
						transformIdx,
						1);
				}
			}
		}

		const bool mustReallocateMaterialBuffer = m_instancedMaterials != nullptr &&
			m_instancedMaterials->GetNumElements() < m_instancedMaterialIndexes.size();

		const uint32_t requestedMaterialBufferElements = util::RoundUpToNearestMultiple(
			util::CheckedCast<uint32_t>(m_instancedMaterialIndexes.size()),
			k_numBlocksPerAllocation);

		if ((mustReallocateMaterialBuffer || m_instancedMaterials == nullptr) && requestedMaterialBufferElements > 0)
		{
			m_instancedMaterials = CreateInstancedBuffer<InstancedPBRMetallicRoughnessData>(
				InstancedPBRMetallicRoughnessData::s_shaderName,
				requestedMaterialBufferElements);

			if (mustReallocateMaterialBuffer)
			{
				LOG_WARNING("gr::BatchManager: Material instance buffer is being reallocated");

				for (auto& materialRecord : m_instancedMaterialIndexes)
				{
					SEAssert(materialRecord.second.m_refCount >= 1, "Invalid ref count");

					gr::RenderDataID materialID = materialRecord.first;
					const uint32_t materialIdx = materialRecord.second.m_index;

					gr::Material::MaterialInstanceData const& materialData =
						renderData.GetObjectData<gr::Material::MaterialInstanceData>(materialID);

					gr::Material::CommitMaterialInstanceData(
						m_instancedMaterials.get(),
						&materialData,
						materialIdx);
				}
			}
		}

		// Update dirty instanced transform data:
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

		// Update dirty instanced material data:
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
					SEAssert(m_instancedMaterialIndexes.contains(dirtyMaterialID),
						"RenderDataID has not been registered for instancing indexes");

					const uint32_t materialIdx = m_instancedMaterialIndexes.at(dirtyMaterialID).m_index;

					gr::Material::MaterialInstanceData const& materialData =
						renderData.GetObjectData<gr::Material::MaterialInstanceData>(dirtyMaterialID);

					gr::Material::CommitMaterialInstanceData(
						m_instancedMaterials.get(),
						&materialData,
						materialIdx);
				}
				++dirtyMaterialItr;
			}
		}
	}


	std::vector<re::Batch> BatchManager::GetSceneBatches(
		gr::RenderDataManager const& renderData, 
		std::vector<gr::RenderDataID> const& renderDataIDs,
		uint8_t bufferTypeMask /*= (InstanceType::Transform | InstanceType::Material)*/) const
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
				[](BatchMetadata const& a, BatchMetadata const& b) { return (a.m_batchHash > b.m_batchHash); });

			size_t unmergedIdx = 0;
			do
			{
				re::Batch const& cachedBatch = m_permanentCachedBatches[batchMetadata[unmergedIdx].m_cacheIndex];

				// Add the first batch in the sequence to our final list. We duplicate the batch, as the cached batches
				// have a permanent Lifetime
				batches.emplace_back(re::Batch::Duplicate(cachedBatch, re::Batch::Lifetime::SingleFrame));

				const uint64_t curBatchHash = batchMetadata[unmergedIdx].m_batchHash;

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
					SEAssert(m_instancedMaterialIndexes.contains(batchMetadata[unmergedSrcIdx].m_renderDataID),
						"RenderDataID is not registered for an instanced material index");

					const uint32_t transformIdx =
						m_instancedTransformIndexes.at(batchMetadata[unmergedSrcIdx].m_transformID).m_index;

					const uint32_t materialIdx =
						m_instancedMaterialIndexes.at(batchMetadata[unmergedSrcIdx].m_renderDataID).m_index;

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
						batches.back().SetBuffer(m_instancedMaterials);
					}
				}

			} while (unmergedIdx < batchMetadata.size());
		}

		return batches;
	}


	std::vector<re::Batch> BatchManager::GetAllSceneBatches(gr::RenderDataManager const& renderData,
		uint8_t bufferTypeMask /*= (InstanceType::Transform | InstanceType::Material)*/) const
	{
		std::vector<gr::RenderDataID> allRenderDataIDs;
		allRenderDataIDs.reserve(m_renderDataIDToBatchMetadata.size());

		for (auto const& metadata : m_renderDataIDToBatchMetadata)
		{
			allRenderDataIDs.emplace_back(metadata.first);
		}

		return GetSceneBatches(renderData, allRenderDataIDs, bufferTypeMask);
	}
}