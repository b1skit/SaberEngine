// © 2024 Adam Badke. All rights reserved.
#include "BufferView.h"
#include "GraphicsSystem_BatchManager.h"
#include "GraphicsSystemCommon.h"
#include "GraphicsSystemManager.h"
#include "IndexedBuffer.h"
#include "Material.h"
#include "RenderDataManager.h"
#include "RenderManager.h"

#include "Core/ProfilingMarkers.h"

#include "Shaders/Common/InstancingParams.h"


namespace gr
{
	BatchManagerGraphicsSystem::BatchManagerGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_viewCullingResults(nullptr)
	{
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
		SEBeginCPUEvent("BatchManagerGraphicsSystem::PreRender");

		SEAssert(m_permanentCachedBatches.size() == m_renderDataIDToBatchMetadata.size() &&
			m_permanentCachedBatches.size() == m_cacheIdxToRenderDataID.size(),
			"Batch cache and batch maps are out of sync");

		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Remove deleted batches
		SEBeginCPUEvent("Remove deleted batches");
		std::vector<gr::RenderDataID> const* deletedMeshPrimIDs =
			renderData.GetIDsWithDeletedData<gr::MeshPrimitive::RenderData>();
		if (deletedMeshPrimIDs)
		{
			for (gr::RenderDataID renderDataIDToDelete : *deletedMeshPrimIDs)
			{
				if (m_renderDataIDToBatchMetadata.contains(renderDataIDToDelete))
				{
					auto deletedIDMetadataItr = m_renderDataIDToBatchMetadata.find(renderDataIDToDelete);

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
				}
			}
		}
		SEEndCPUEvent(); // Remove deleted batches

		// Create batches for newly added IDs
		SEBeginCPUEvent("Create new batches");
		std::vector<gr::RenderDataID> const* newMeshPrimIDs =
			renderData.GetIDsWithNewData<gr::MeshPrimitive::RenderData>();
		if (newMeshPrimIDs)
		{
			for (auto const& newMeshPrimDataItr : gr::IDAdapter(renderData, *newMeshPrimIDs))
			{
				const gr::RenderDataID newMeshPrimID = newMeshPrimDataItr->GetRenderDataID();

				if (gr::HasFeature(gr::RenderObjectFeature::IsMeshPrimitiveConcept, newMeshPrimDataItr->GetFeatureBits()))
				{
					gr::MeshPrimitive::RenderData const& meshPrimRenderData =
						newMeshPrimDataItr->Get<gr::MeshPrimitive::RenderData>();
					gr::Material::MaterialInstanceRenderData const& materialRenderData =
						newMeshPrimDataItr->Get<gr::Material::MaterialInstanceRenderData>();

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
							.m_matEffectID = matEffectID,
							.m_cacheIndex = newBatchIdx
						});
				}
			}
		}
		SEEndCPUEvent(); // Create new batches

		BuildViewBatches(renderData.GetInstancingIndexedBufferManager());

		SEEndCPUEvent(); // BatchManagerGraphicsSystem::PreRender
	}


	void BatchManagerGraphicsSystem::EndOfFrame()
	{
		m_viewBatches.clear(); // Make sure we're not hanging on to any Buffers etc
		m_allBatches.clear();
	}

	
	void BatchManagerGraphicsSystem::BuildViewBatches(gr::IndexedBufferManager& ibm)
	{
		SEBeginCPUEvent("BatchManagerGraphicsSystem::BuildViewBatches");

		SEAssert(m_allBatches.empty(), "Batch vectors should have been cleared");

		std::unordered_set<gr::RenderDataID> seenIDs; // Ensure no duplicates in m_allBatches

		for (auto const& viewAndCulledIDs : *m_viewCullingResults)
		{
			SEBeginCPUEvent("viewAndCulledIDs entry");

			SEBeginCPUEvent("Copy batch metadata");
			gr::Camera::View const& curView = viewAndCulledIDs.first;
			std::vector<gr::RenderDataID> const& renderDataIDs = viewAndCulledIDs.second;

			// Assemble the batch metadata for the requested RenderDataIDs:
			std::vector<BatchMetadata const*> batchMetadata;
			batchMetadata.reserve(renderDataIDs.size());
			for (size_t i = 0; i < renderDataIDs.size(); i++)
			{
				SEAssert(m_renderDataIDToBatchMetadata.contains(renderDataIDs[i]),
					"Batch with the given ID does not exist");

				batchMetadata.emplace_back(&m_renderDataIDToBatchMetadata.at(renderDataIDs[i]));
			}
			SEAssert(m_viewBatches[curView].empty(), "Batch vectors should have been cleared");

			SEEndCPUEvent(); // Copy batch metadata

			// Assemble a list of instanced batches:
			SEBeginCPUEvent("Assemble batches");
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
					SEBeginCPUEvent("Duplicate batches");
					re::Batch const& cachedBatch = m_permanentCachedBatches[batchMetadata[unmergedIdx]->m_cacheIndex];

					const bool isFirstTimeSeen = seenIDs.emplace(batchMetadata[unmergedIdx]->m_renderDataID).second;

					// Add the first batch in the sequence to our final list. We duplicate the batch, as cached batches
					// have a permanent Lifetime
					batches.emplace_back(re::Batch::Duplicate(cachedBatch, re::Lifetime::SingleFrame));
					if (isFirstTimeSeen)
					{
						m_allBatches.emplace_back(re::Batch::Duplicate(cachedBatch, re::Lifetime::SingleFrame));
					}
					SEEndCPUEvent(); // Duplicate batches

					// Find the index of the last batch with a matching hash in the sequence:
					SEBeginCPUEvent("Find mergeable instance");
					const uint64_t curBatchHash = batchMetadata[unmergedIdx]->m_batchHash;
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
					SEEndCPUEvent(); // Find mergeable instance

					// Attach the instance and LUT buffers:
					SEBeginCPUEvent("Attach instance buffers");
					effect::Effect const* batchEffect = effectDB.GetEffect(batches.back().GetEffectID());

					static const util::HashKey k_transformBufferNameHash("InstancedTransformParams");
					static const util::HashKey k_materialBufferNameHash("InstancedPBRMetallicRoughnessParams");

					bool setInstanceBuffer = false;
					if (batchEffect->UsesBuffer(k_transformBufferNameHash))
					{
						batches.back().SetBuffer(ibm.GetIndexedBufferInput(
							k_transformBufferNameHash,
							"InstancedTransformParams"));
						setInstanceBuffer = true;
					}
					if (batchEffect->UsesBuffer(k_materialBufferNameHash))
					{
						batches.back().SetBuffer(ibm.GetIndexedBufferInput(
							k_materialBufferNameHash,
							"InstancedPBRMetallicRoughnessParams"));
						setInstanceBuffer = true;
					}

					if (setInstanceBuffer)
					{
						SEBeginCPUEvent("GetSingleFrameLUTBufferInput");

						// Use a view of our batch metadata to get the list of RenderDataIDs for each instance:
						std::ranges::range auto&& instancedBatchView = batchMetadata
							| std::views::drop(instanceStartIdx)
							| std::views::take(numInstances)
							| std::ranges::views::transform([](BatchMetadata const* batchMetadata) -> gr::RenderDataID
								{
									return batchMetadata->m_renderDataID;
								});

						batches.back().SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(instancedBatchView));

						SEEndCPUEvent(); // GetSingleFrameLUTBufferInput
					}
					SEEndCPUEvent(); // Attach instance buffers

				} while (unmergedIdx < batchMetadata.size());
			}

			SEEndCPUEvent(); // Assemble batches
			SEEndCPUEvent(); // viewAndCulledIDs entry
		}

		SEEndCPUEvent();
	}
}