// © 2024 Adam Badke. All rights reserved.
#include "Batch.h"
#include "BatchBuilder.h"
#include "BatchFactories.h"
#include "EffectDB.h"
#include "GraphicsSystem_BatchManager.h"
#include "GraphicsSystemCommon.h"
#include "GraphicsSystemManager.h"
#include "IndexedBuffer.h"
#include "Material.h"
#include "RenderDataManager.h"
#include "RenderManager.h"

#include "Core/ProfilingMarkers.h"

#include "Renderer/Shaders/Common/InstancingParams.h"
#include "Renderer/Shaders/Common/MaterialParams.h"
#include "Renderer/Shaders/Common/TransformParams.h"


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
						m_permanentCachedBatches[cacheIdxToReplace] = m_permanentCachedBatches[cacheIdxToMove];

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

		// Create/update batches for new/dirty objects
		SEBeginCPUEvent("Create/update batches");

		std::vector<gr::RenderDataID> const& dirtyIDs =
			renderData.GetIDsWithAnyDirtyData<gr::MeshPrimitive::RenderData, gr::Material::MaterialInstanceRenderData>(
				gr::RenderObjectFeature::IsMeshPrimitiveConcept);

		if (!dirtyIDs.empty())
		{
			for (auto const& itr : gr::IDAdapter(renderData, dirtyIDs))
			{
				const gr::RenderDataID renderDataID = itr->GetRenderDataID();

				SEAssert(itr->HasObjectData<gr::MeshPrimitive::RenderData>() &&
					itr->HasObjectData<gr::Material::MaterialInstanceRenderData>() &&
					gr::HasFeature(gr::RenderObjectFeature::IsMeshPrimitiveConcept, itr->GetFeatureBits()),
					"Render data object does not have the expected configuration");

				gr::MeshPrimitive::RenderData const& meshPrimRenderData = itr->Get<gr::MeshPrimitive::RenderData>();
				
				gr::Material::MaterialInstanceRenderData const& materialRenderData =
					itr->Get<gr::Material::MaterialInstanceRenderData>();

				// Get any animated vertex streams overrides, if they exist
				re::Batch::VertexStreamOverride const* vertexStreamOverrides = nullptr;
				auto const& animatedStreams = m_animatedVertexStreams->find(renderDataID);
				if (animatedStreams != m_animatedVertexStreams->end())
				{
					vertexStreamOverrides = &animatedStreams->second;
				}
				SEAssert(!meshPrimRenderData.m_hasMorphTargets || vertexStreamOverrides,
					"Morph target flag and vertex stream override results are out of sync");

				auto batchMetadataItr = m_renderDataIDToBatchMetadata.find(renderDataID);
				if (batchMetadataItr == m_renderDataIDToBatchMetadata.end()) // Add a new batch
				{
					const size_t newBatchIdx = m_permanentCachedBatches.size();

					m_permanentCachedBatches.emplace_back(gr::RasterBatchBuilder::CreateInstance(
						renderDataID, renderData, grutil::BuildInstancedRasterBatch, vertexStreamOverrides).Build());

					const uint64_t batchHash = m_permanentCachedBatches.back()->GetDataHash();

					// Update the metadata:
					m_cacheIdxToRenderDataID.emplace(newBatchIdx, renderDataID);

					const EffectID matEffectID = materialRenderData.m_effectID;

					m_renderDataIDToBatchMetadata.emplace(
						renderDataID,
						BatchMetadata{
							.m_batchHash = batchHash,
							.m_renderDataID = renderDataID,
							.m_matEffectID = matEffectID,
							.m_cacheIndex = newBatchIdx
						});
				}
				else if (itr->IsDirty<gr::Material::MaterialInstanceRenderData>())// Update existing batch
				{
					BatchMetadata& batchMetadata = m_renderDataIDToBatchMetadata.at(renderDataID);

					m_permanentCachedBatches[batchMetadata.m_cacheIndex] = gr::RasterBatchBuilder::CreateInstance(
							renderDataID, renderData, grutil::BuildInstancedRasterBatch, vertexStreamOverrides).Build();

					// Update the batch metadata:
					batchMetadata.m_batchHash = m_permanentCachedBatches[batchMetadata.m_cacheIndex]->GetDataHash();
					batchMetadata.m_matEffectID = materialRenderData.m_effectID;
				}
			}
		}
		SEEndCPUEvent(); // Create/update batches for new/dirty objects

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

			gr::Camera::View const& curView = viewAndCulledIDs.first;
			std::vector<gr::RenderDataID> const& renderDataIDs = viewAndCulledIDs.second;

			SEAssert(m_viewBatches[curView].empty(), "Batch vectors should have been cleared");

			// Assemble a list of instanced batches:
			SEBeginCPUEvent("Assemble batches");
			std::vector<gr::BatchHandle>& viewBatches = m_viewBatches[curView];
			viewBatches.reserve(renderDataIDs.size());

			effect::EffectDB const& effectDB = re::RenderManager::Get()->GetEffectDB();

			for (gr::RenderDataID renderDataID : renderDataIDs)
			{
				SEBeginCPUEvent("Duplicate batches");

				BatchMetadata const& batchMetadata = m_renderDataIDToBatchMetadata.at(renderDataID);

				const gr::BatchHandle cachedBatch = m_permanentCachedBatches[batchMetadata.m_cacheIndex];

				const bool isFirstTimeSeen = seenIDs.emplace(batchMetadata.m_renderDataID).second;

				// Add the first batch in the sequence to our final list. We duplicate the batch, as cached batches
				// have a permanent Lifetime
				viewBatches.emplace_back(cachedBatch);
				if (isFirstTimeSeen)
				{
					m_allBatches.emplace_back(cachedBatch);
				}
				SEEndCPUEvent(); // Duplicate batches
			}

			SEEndCPUEvent(); // ssemble batches
			SEEndCPUEvent(); // viewAndCulledIDs entry
		}

		SEEndCPUEvent(); // BatchManagerGraphicsSystem::BuildViewBatches
	}
}