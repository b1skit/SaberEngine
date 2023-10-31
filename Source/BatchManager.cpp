// © 2023 Adam Badke. All rights reserved.
#include "BatchManager.h"
#include "Mesh.h"
#include "CastUtils.h"


namespace re
{
	std::vector<re::Batch> BatchManager::BuildBatches(std::vector<std::shared_ptr<gr::Mesh>> const& meshes)
	{
		struct BatchSortMetadata
		{
			size_t m_meshIdx;
			size_t m_meshPrimitiveIdx;
			re::Batch m_batch;
		};

		std::vector<BatchSortMetadata> unmergedBatches;
		unmergedBatches.reserve(meshes.size());
		for (size_t meshIdx = 0; meshIdx < meshes.size(); meshIdx++)
		{
			for (size_t meshPrimitiveIdx = 0; meshPrimitiveIdx < meshes[meshIdx]->GetMeshPrimitives().size(); meshPrimitiveIdx++)
			{
				gr::MeshPrimitive const* meshPrimitive = meshes[meshIdx]->GetMeshPrimitives()[meshPrimitiveIdx].get();

				unmergedBatches.emplace_back(
					BatchSortMetadata
					{
						meshIdx,
						meshPrimitiveIdx,
						re::Batch(re::Batch::Lifetime::SingleFrame, meshPrimitive, meshPrimitive->GetMeshMaterial())
					});
			}
		}

		// Sort the batches:
		std::sort(
			unmergedBatches.begin(),
			unmergedBatches.end(),
			[](BatchSortMetadata const& a, BatchSortMetadata const& b) -> bool
			{ return (a.m_batch.GetDataHash() > b.m_batch.GetDataHash()); }
		);

		// Assemble a list of merged batches:
		std::vector<re::Batch> mergedBatches;
		mergedBatches.reserve(meshes.size());
		size_t unmergedIdx = 0;
		do
		{
			// Add the first batch in the sequence to our final list:
			mergedBatches.emplace_back(std::move(unmergedBatches[unmergedIdx].m_batch));

			const uint64_t curBatchHash = mergedBatches.back().GetDataHash();

			// Find the index of the last batch with a matching hash in the sequence:
			const size_t instanceStartIdx = unmergedIdx++;
			while (unmergedIdx < unmergedBatches.size() &&
				unmergedBatches[unmergedIdx].m_batch.GetDataHash() == curBatchHash)
			{
				unmergedIdx++;
			}

			// Compute and set the number of instances in the batch:
			const uint32_t numInstances = util::CheckedCast<uint32_t, size_t>(unmergedIdx - instanceStartIdx);

			mergedBatches.back().SetInstanceCount(numInstances);


			// Build/attach instanced PBs:
			std::vector<gr::Transform*> instanceTransforms;
			instanceTransforms.reserve(numInstances);

			for (size_t instanceOffset = 0; instanceOffset < numInstances; instanceOffset++)
			{
				const size_t unmergedSrcIdx = instanceStartIdx + instanceOffset;
				const size_t srcMeshIdx = unmergedBatches[unmergedSrcIdx].m_meshIdx;
				
				gr::Transform* meshTransform = meshes[srcMeshIdx]->GetTransform();

				// Add the Transform to our list
				instanceTransforms.emplace_back(meshTransform);
			}
			mergedBatches.back().SetParameterBlock(gr::Mesh::CreateInstancedMeshParamsData(instanceTransforms));


		} while (unmergedIdx < unmergedBatches.size());

		return mergedBatches;
	}
}