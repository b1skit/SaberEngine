// © 2024 Adam Badke. All rights reserved.
#include "GraphicsSystem_BatchManager.h"
#include "GraphicsSystemCommon.h"
#include "GraphicsSystemManager.h"
#include "RenderDataManager.h"
#include "RenderManager.h"
#include "Sampler.h"

#include "Core/InvPtr.h"
#include "Core/Logger.h"

#include "Core/Util/MathUtils.h"


struct RefCountedIndex
{
	uint32_t m_index;
	uint32_t m_refCount;
};

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


	re::BufferInput CreateInstanceIndexBuffer(
		std::vector<InstanceIndices> const& instanceIndices)
	{
		SEAssert(instanceIndices.size() < InstanceIndexData::k_maxInstances,
			"Too many instances, consider increasing INSTANCE_ARRAY_SIZE/k_maxInstances");

		InstanceIndexData instanceIndexBufferData{};

		memcpy(&instanceIndexBufferData.g_instanceIndices,
			instanceIndices.data(),
			sizeof(InstanceIndices) * instanceIndices.size());

		return re::BufferInput(
			InstanceIndexData::s_shaderName,
			re::Buffer::Create(
				InstanceIndexData::s_shaderName,
				instanceIndexBufferData,
				re::Buffer::BufferParams{
					.m_lifetime = re::Lifetime::SingleFrame,
					.m_stagingPool = re::Buffer::StagingPool::Temporary,
					.m_memPoolPreference = re::Buffer::UploadHeap,
					.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
					.m_usageMask = re::Buffer::Constant,
				}));
	}


	template<typename T>
	void AssignInstancingIndex(
		std::unordered_map<T, RefCountedIndex>& indexMap,
		std::vector<uint32_t>& freeIndexes,
		T newID)
	{
		// Transforms can be shared; We only need a single index in our array
		if (indexMap.contains(newID))
		{
			RefCountedIndex& refCountedIndex = indexMap.at(newID);
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
				RefCountedIndex{
					.m_index = newIndex,
					.m_refCount = 1
				});
		}
	}


	template<typename T>
	void FreeInstancingIndex(
		std::unordered_map<T, RefCountedIndex>& indexMap,
		std::vector<uint32_t>& freeIndexes,
		T idToFree)
	{
		SEAssert(indexMap.contains(idToFree), "ID has not been assigned an index");

		RefCountedIndex& refCountedIndex = indexMap.at(idToFree);
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
	BatchManagerGraphicsSystem::BatchManagerGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_viewCullingResults(nullptr)
	{
		m_instancedTransformIndexes.reserve(k_numBlocksPerAllocation);
		m_freeTransformIndexes.reserve(k_numBlocksPerAllocation);
	}


	void BatchManagerGraphicsSystem::RegisterInputs()
	{
		RegisterDataInput(k_cullingDataInput);
		RegisterDataInput(k_animatedVertexStreamsInput);
	}


	void BatchManagerGraphicsSystem::RegisterOutputs()
	{
		RegisterDataOutput(k_viewBatchesDataOutput, &m_viewBatches);
		RegisterDataOutput(k_allBatchesDataOutput, &m_allBatches);
	}


	void BatchManagerGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline,
		TextureDependencies const&,
		BufferDependencies const&,
		DataDependencies const& dataDependencies)
	{
		m_viewCullingResults = GetDataDependency<ViewCullingResults>(k_cullingDataInput, dataDependencies);
		SEAssert(m_viewCullingResults, "View culling results cannot (currently) be null");

		m_animatedVertexStreams = 
			GetDataDependency<AnimatedVertexStreams>(k_animatedVertexStreamsInput, dataDependencies);
		SEAssert(m_animatedVertexStreams, "Animated vertex streams map cannot (currently) be null");
	}


	void BatchManagerGraphicsSystem::PreRender()
	{
		SEAssert(m_permanentCachedBatches.size() == m_renderDataIDToBatchMetadata.size() &&
			m_permanentCachedBatches.size() == m_cacheIdxToRenderDataID.size(),
			"Batch cache and batch maps are out of sync");

		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Remove deleted batches
		std::vector<gr::RenderDataID> const* deletedMeshPrimIDs =
			renderData.GetIDsWithDeletedData<gr::MeshPrimitive::RenderData>();
		if (deletedMeshPrimIDs)
		{
			for (gr::RenderDataID renderDataIDToDelete : *deletedMeshPrimIDs)
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

					const EffectID matEffectID = m_renderDataIDToBatchMetadata.at(renderDataIDToDelete).m_matEffectID;

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
						m_materialInstanceMetadata[matEffectID].m_instancedMaterialIndexes,
						m_materialInstanceMetadata[matEffectID].m_freeInstancedMaterialIndexes,
						renderDataIDToDelete);
				}
			}
		}


		// Create batches for newly added IDs
		std::vector<gr::RenderDataID> const* newMeshPrimIDs =
			renderData.GetIDsWithNewData<gr::MeshPrimitive::RenderData>();
		if (newMeshPrimIDs)
		{
			auto newMeshPrimDataItr = renderData.IDBegin(*newMeshPrimIDs);
			auto const& newMeshPrimDataItrEnd = renderData.IDEnd(*newMeshPrimIDs);
			while (newMeshPrimDataItr != newMeshPrimDataItrEnd)
			{
				const gr::RenderDataID newMeshPrimID = newMeshPrimDataItr.GetRenderDataID();

				if (gr::HasFeature(gr::RenderObjectFeature::IsMeshPrimitive, newMeshPrimDataItr.GetFeatureBits()))
				{
					gr::MeshPrimitive::RenderData const& meshPrimRenderData =
						newMeshPrimDataItr.Get<gr::MeshPrimitive::RenderData>();
					gr::Material::MaterialInstanceRenderData const& materialRenderData =
						newMeshPrimDataItr.Get<gr::Material::MaterialInstanceRenderData>();

					const gr::TransformID newBatchTransformID = newMeshPrimDataItr.GetTransformID();
					const size_t newBatchIdx = m_permanentCachedBatches.size();

					// Get any animated vertex streams overrides, if they exist
					re::Batch::VertexStreamOverride const* vertexStreamOverrides = nullptr;
					auto const& animatedStreams = m_animatedVertexStreams->find(newMeshPrimID);
					if (animatedStreams != m_animatedVertexStreams->end())
					{
						vertexStreamOverrides = &animatedStreams->second;
					}
					SEAssert(!meshPrimRenderData.m_hasMorphTargets || vertexStreamOverrides,
						"Morph target flag and vertex stream override results are out of sync");

					m_permanentCachedBatches.emplace_back(
						re::Batch(re::Lifetime::Permanent, meshPrimRenderData, &materialRenderData, vertexStreamOverrides));

					const uint64_t batchHash = m_permanentCachedBatches.back().GetDataHash();

					// Update the metadata:
					m_cacheIdxToRenderDataID.emplace(newBatchIdx, newMeshPrimID);

					const EffectID matEffectID = materialRenderData.m_effectID;

					m_renderDataIDToBatchMetadata.emplace(
						newMeshPrimID,
						BatchMetadata{
							.m_batchHash = batchHash,
							.m_renderDataID = newMeshPrimID,
							.m_transformID = newBatchTransformID,
							.m_matEffectID = matEffectID,
							.m_cacheIndex = newBatchIdx
						});

					AssignInstancingIndex(
						m_instancedTransformIndexes,
						m_freeTransformIndexes,
						newBatchTransformID);

					AssignInstancingIndex( // Note: [Inserts] if matEffectID has not been seen before
						m_materialInstanceMetadata[matEffectID].m_instancedMaterialIndexes,
						m_materialInstanceMetadata[matEffectID].m_freeInstancedMaterialIndexes,
						newMeshPrimID);
				}

				++newMeshPrimDataItr;
			}
		}


		// Create/grow our permanent Transform instance buffers:
		const bool mustReallocateTransformBuffer = m_instancedTransforms.GetBuffer() != nullptr &&
			m_instancedTransforms.GetBuffer()->GetArraySize() < m_instancedTransformIndexes.size();

		const uint32_t requestedTransformBufferElements = util::RoundUpToNearestMultiple(
			util::CheckedCast<uint32_t>(m_instancedTransformIndexes.size()),
			k_numBlocksPerAllocation);

		if ((mustReallocateTransformBuffer || m_instancedTransforms.GetBuffer() == nullptr) &&
			requestedTransformBufferElements > 0)
		{
			m_instancedTransforms = re::BufferInput(
				InstancedTransformData::s_shaderName,
				re::Buffer::CreateUncommittedArray<InstancedTransformData>(
					k_instancedTransformBufferName,
					re::Buffer::BufferParams{
						.m_lifetime = re::Lifetime::Permanent,
						.m_stagingPool = re::Buffer::StagingPool::Permanent,
						.m_memPoolPreference = re::Buffer::UploadHeap,
						.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
						.m_usageMask = re::Buffer::Structured,
						.m_arraySize = requestedTransformBufferElements,
					}));

			// If we reallocated, re-copy all of the data to the new buffer
			if (mustReallocateTransformBuffer)
			{
				LOG_WARNING("gr::BatchManagerGraphicsSystem: Transform instance buffer is being reallocated");

				for (auto& transformRecord : m_instancedTransformIndexes)
				{
					SEAssert(transformRecord.second.m_refCount >= 1, "Invalid ref count");

					gr::TransformID transformID = transformRecord.first;
					const uint32_t transformIdx = transformRecord.second.m_index;

					gr::Transform::RenderData const& transformData =
						renderData.GetTransformDataFromTransformID(transformID);

					InstancedTransformData const& transformParams =
						gr::Transform::CreateInstancedTransformData(transformData);

					m_instancedTransforms.GetBuffer()->Commit(
						&transformParams,
						transformIdx,
						1);
				}
			}
		}

		// Create/grow our permanent Material instance buffers:
		for (auto& materialMetadataEntry : m_materialInstanceMetadata)
		{
			const EffectID matEffectID = materialMetadataEntry.first;
			MaterialInstanceMetadata& matInstMeta = materialMetadataEntry.second;

			const bool mustReallocateMaterialBuffer =
				matInstMeta.m_instancedMaterials.GetBuffer() != nullptr &&
				matInstMeta.m_instancedMaterials.GetBuffer()->GetArraySize() < matInstMeta.m_instancedMaterialIndexes.size();

			const uint32_t requestedMaterialBufferElements = util::RoundUpToNearestMultiple(
				util::CheckedCast<uint32_t>(matInstMeta.m_instancedMaterialIndexes.size()),
				k_numBlocksPerAllocation);

			if ((mustReallocateMaterialBuffer || matInstMeta.m_instancedMaterials.GetBuffer() == nullptr) &&
				requestedMaterialBufferElements > 0)
			{
				matInstMeta.m_instancedMaterials =
					gr::Material::ReserveInstancedBuffer(matEffectID, requestedMaterialBufferElements);

				if (mustReallocateMaterialBuffer)
				{
					LOG_WARNING("BatchManagerGraphicsSystem: Effect \"%s\" Material instance buffer is being reallocated",
						re::RenderManager::Get()->GetEffectDB().GetEffect(matEffectID)->GetName());

					for (auto& materialRecord : matInstMeta.m_instancedMaterialIndexes)
					{
						SEAssert(materialRecord.second.m_refCount >= 1, "Invalid ref count");

						const gr::RenderDataID materialID = materialRecord.first;
						const uint32_t materialIdx = materialRecord.second.m_index;

						gr::Material::MaterialInstanceRenderData const& materialData =
							renderData.GetObjectData<gr::Material::MaterialInstanceRenderData>(materialID);

						gr::Material::CommitMaterialInstanceData(
							matInstMeta.m_instancedMaterials.GetBuffer(),
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

				m_instancedTransforms.GetBuffer()->Commit(
					&transformParams,
					transformIdx,
					1);
			}
		}

		// Update dirty instanced Material data:
		if (renderData.HasObjectData<gr::Material::MaterialInstanceRenderData>())
		{
			std::vector<gr::RenderDataID> const* dirtyMaterials =
				renderData.GetIDsWithDirtyData<gr::Material::MaterialInstanceRenderData>();
			if (dirtyMaterials)
			{
				auto dirtyMaterialItr = renderData.IDBegin(*dirtyMaterials);
				auto const& dirtyMaterialItrEnd = renderData.IDEnd(*dirtyMaterials);
				while (dirtyMaterialItr != dirtyMaterialItrEnd)
				{
					const gr::RenderDataID dirtyMaterialID = dirtyMaterialItr.GetRenderDataID();

					if (gr::HasFeature(gr::RenderObjectFeature::IsMeshPrimitive, dirtyMaterialItr.GetFeatureBits()))
					{
						MaterialInstanceMetadata& matInstMeta =
							m_materialInstanceMetadata[m_renderDataIDToBatchMetadata.at(dirtyMaterialID).m_matEffectID];

						SEAssert(matInstMeta.m_instancedMaterialIndexes.contains(dirtyMaterialID),
							"RenderDataID has not been registered for instancing indexes");

						const uint32_t materialIdx = matInstMeta.m_instancedMaterialIndexes.at(dirtyMaterialID).m_index;

						gr::Material::MaterialInstanceRenderData const& materialData =
							renderData.GetObjectData<gr::Material::MaterialInstanceRenderData>(dirtyMaterialID);

						gr::Material::CommitMaterialInstanceData(
							matInstMeta.m_instancedMaterials.GetBuffer(),
							&materialData,
							materialIdx);

						// Recreate the associated Batch in case something on the Material that modifies the Batch
						// behavior has changed (e.g. filter bits: shadow casting enabled/disabled)
						re::Batch& permanentBatch =
							m_permanentCachedBatches.at(m_renderDataIDToBatchMetadata.at(dirtyMaterialID).m_cacheIndex);
						
						permanentBatch = re::Batch(
							re::Lifetime::Permanent,
							renderData.GetObjectData<gr::MeshPrimitive::RenderData>(dirtyMaterialID),
							&materialData,
							&permanentBatch.GetGraphicsParams().m_vertexBuffers); // Keep any vertex stream overrides
					}
					++dirtyMaterialItr;
				}
			}
		}

		BuildViewBatches();
	}


	void BatchManagerGraphicsSystem::EndOfFrame()
	{
		m_viewBatches.clear(); // Make sure we're not hanging on to any Buffers etc
		m_allBatches.clear();
		m_instanceIndiciesBuffers.clear();
	}

	
	void BatchManagerGraphicsSystem::BuildViewBatches()
	{
		SEAssert(m_allBatches.empty(), "Batch vectors should have been cleared");

		std::unordered_set<gr::RenderDataID> seenIDs; // Ensure no duplicates in m_allBatches

		for (auto const& viewAndCulledIDs : *m_viewCullingResults)
		{
			gr::Camera::View const& curView = viewAndCulledIDs.first;
			std::vector<gr::RenderDataID> const& renderDataIDs = viewAndCulledIDs.second;

			// Copy the batch metadata for the requested RenderDataIDs:
			std::vector<BatchMetadata const*> batchMetadata;
			batchMetadata.reserve(renderDataIDs.size());
			for (size_t i = 0; i < renderDataIDs.size(); i++)
			{
				SEAssert(m_renderDataIDToBatchMetadata.contains(renderDataIDs[i]),
					"Batch with the given ID does not exist");

				batchMetadata.emplace_back(&m_renderDataIDToBatchMetadata.at(renderDataIDs[i]));
			}

			SEAssert(m_viewBatches[curView].empty(), "Batch vectors should have been cleared");

			// Assemble a list of instanced batches:
			std::vector<re::Batch>& batches = m_viewBatches[curView];
			batches.reserve(batchMetadata.size());			

			effect::EffectDB const& effectDB = re::RenderManager::Get()->GetEffectDB();

			if (!batchMetadata.empty())
			{
				// Sort the batch metadata:
				std::sort(batchMetadata.begin(), batchMetadata.end(),
					[](BatchMetadata const* a, BatchMetadata const* b) { return (a->m_batchHash < b->m_batchHash); });

				size_t unmergedIdx = 0;
				do
				{
					re::Batch const& cachedBatch = m_permanentCachedBatches[batchMetadata[unmergedIdx]->m_cacheIndex];

					const bool isFirstTimeSeen = seenIDs.emplace(batchMetadata[unmergedIdx]->m_renderDataID).second;

					// Add the first batch in the sequence to our final list. We duplicate the batch, as cached batches
					// have a permanent Lifetime
					batches.emplace_back(re::Batch::Duplicate(cachedBatch, re::Lifetime::SingleFrame));
					if (isFirstTimeSeen)
					{
						m_allBatches.emplace_back(re::Batch::Duplicate(cachedBatch, re::Lifetime::SingleFrame));
					}

					const uint64_t curBatchHash = batchMetadata[unmergedIdx]->m_batchHash;

					// Obtain the Material instance metadata while we still have the current unmergedIdx		
					MaterialInstanceMetadata const& matInstMeta =
						m_materialInstanceMetadata.at(batchMetadata[unmergedIdx]->m_matEffectID);

					// Find the index of the last batch with a matching hash in the sequence:
					const size_t instanceStartIdx = unmergedIdx++;
					while (unmergedIdx < batchMetadata.size() &&
						batchMetadata[unmergedIdx]->m_batchHash == curBatchHash)
					{
						unmergedIdx++;
					}

					// Compute and set the number of instances in the batch:
					const uint32_t numInstances = util::CheckedCast<uint32_t, size_t>(unmergedIdx - instanceStartIdx);
					batches.back().SetInstanceCount(numInstances);
					if (isFirstTimeSeen)
					{
						m_allBatches.back().SetInstanceCount(numInstances);
					}
					
					// Gather the data we need to build our instanced buffers:
					std::vector<InstanceIndices> instanceIndices;
					instanceIndices.reserve(numInstances);

					util::HashKey instanceIdxsHash = 0; // Hash the Buffer contents so we can reuse buffers

					for (size_t instanceOffset = 0; instanceOffset < numInstances; instanceOffset++)
					{
						const size_t unmergedSrcIdx = instanceStartIdx + instanceOffset;

						SEAssert(m_instancedTransformIndexes.contains(batchMetadata[unmergedSrcIdx]->m_transformID),
							"TransformID is not registered for an instanced transform index");
						SEAssert(matInstMeta.m_instancedMaterialIndexes.contains(batchMetadata[unmergedSrcIdx]->m_renderDataID),
							"RenderDataID is not registered for an instanced material index");

						const uint32_t transformIdx =
							m_instancedTransformIndexes.at(batchMetadata[unmergedSrcIdx]->m_transformID).m_index;

						const uint32_t materialIdx =
							matInstMeta.m_instancedMaterialIndexes.at(batchMetadata[unmergedSrcIdx]->m_renderDataID).m_index;

						instanceIndices.emplace_back(CreateInstanceIndicesEntry(transformIdx, materialIdx));

						util::AddDataToHash(instanceIdxsHash, util::HashDataBytes(&transformIdx, sizeof(uint32_t)));
						util::AddDataToHash(instanceIdxsHash, util::HashDataBytes(&materialIdx, sizeof(uint32_t)));
					}
					SEAssert(!instanceIndices.empty(), "Failed to create any InstanceIndices");

					// Finally, attach our instanced buffers:
					bool setInstancedBuffer = false;

					effect::Effect const* batchEffect = effectDB.GetEffect(batches.back().GetEffectID());

					if (batchEffect->UsesBuffer(m_instancedTransforms.GetBuffer()->GetNameHash()))
					{
						batches.back().SetBuffer(m_instancedTransforms);
						if (isFirstTimeSeen)
						{
							m_allBatches.back().SetBuffer(m_instancedTransforms);
						}						
						setInstancedBuffer = true;
					}
					if (batchEffect->UsesBuffer(matInstMeta.m_instancedMaterials.GetBuffer()->GetNameHash()))
					{
						batches.back().SetBuffer(matInstMeta.m_instancedMaterials);
						if (isFirstTimeSeen)
						{
							m_allBatches.back().SetBuffer(matInstMeta.m_instancedMaterials);
						}						
						setInstancedBuffer = true;
					}

					if (setInstancedBuffer)
					{
						// Minimize the number of singleframe buffers we need to create: We reuse any instancing buffers
						// with the same contents
						if (!m_instanceIndiciesBuffers.contains(instanceIdxsHash))
						{
							m_instanceIndiciesBuffers.emplace(
								instanceIdxsHash, CreateInstanceIndexBuffer(instanceIndices));
						}
						batches.back().SetBuffer(m_instanceIndiciesBuffers.at(instanceIdxsHash));
						if (isFirstTimeSeen)
						{
							m_allBatches.back().SetBuffer(m_instanceIndiciesBuffers.at(instanceIdxsHash));
						}
					}

				} while (unmergedIdx < batchMetadata.size());
			}
		}
	}
}