// © 2023 Adam Badke. All rights reserved.
#include "CastUtils.h"
#include "BatchManager.h"
#include "Material.h"
#include "MeshPrimitive.h"
#include "ProfilingMarkers.h"
#include "RenderData.h"


namespace re
{
	std::vector<re::Batch> BatchManager::BuildBatches(std::vector<std::shared_ptr<gr::Mesh>> const& meshes)
	{
		SEBeginCPUEvent(std::format("BatchManager::BuildBatches ({} meshes)", meshes.size()).c_str());

		struct BatchSortMetadata
		{
			size_t m_meshIdx;
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

		SEEndCPUEvent();

		return mergedBatches;
	}


	std::vector<re::Batch> BatchManager::BuildBatches(gr::RenderData const& renderData)
	{
		SEBeginCPUEvent("BatchManager::BuildBatches");

		struct BatchSortMetadata
		{
			size_t m_transformDataIdx;
			re::Batch m_batch;
		};

		const uint32_t expectedNumMeshPrimitives = renderData.GetNumElementsOfType<gr::MeshPrimitive::RenderData>();

		std::vector<fr::TransformComponent::RenderData const*> transformRenderData;

		// Assemble a list of merged batches:
		std::vector<re::Batch> mergedBatches;
		std::vector<BatchSortMetadata> unmergedBatches;
		unmergedBatches.reserve(expectedNumMeshPrimitives);

		auto renderDataItr = renderData.ObjectBegin<gr::MeshPrimitive::RenderData, gr::Material::RenderData>();
		auto const& renderDataEnd = renderData.ObjectEnd<gr::MeshPrimitive::RenderData, gr::Material::RenderData>();
		while (renderDataItr != renderDataEnd)
		{
			gr::MeshPrimitive::RenderData const& meshPrimRenderData = renderDataItr.Get<gr::MeshPrimitive::RenderData>();
			gr::Material::RenderData const& materialRenderData = renderDataItr.Get<gr::Material::RenderData>();
			
			// Cache a pointer to our Transform render data:
			const size_t transformIdx = transformRenderData.size();
			transformRenderData.emplace_back(&renderDataItr.GetTransformData());
		
			unmergedBatches.emplace_back(
				BatchSortMetadata
				{
					transformIdx,
					re::Batch(re::Batch::Lifetime::SingleFrame, meshPrimRenderData, &materialRenderData)
				});

			++renderDataItr;
		}

		// Sort the batches:
		std::sort(
			unmergedBatches.begin(),
			unmergedBatches.end(),
			[](BatchSortMetadata const& a, BatchSortMetadata const& b) -> bool
			{ return (a.m_batch.GetDataHash() > b.m_batch.GetDataHash()); }
		);

		
		mergedBatches.reserve(unmergedBatches.size());
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
			std::vector<fr::TransformComponent::RenderData const*> instanceTransformRenderData;
			instanceTransformRenderData.reserve(numInstances);

			for (size_t instanceOffset = 0; instanceOffset < numInstances; instanceOffset++)
			{
				const size_t unmergedSrcIdx = instanceStartIdx + instanceOffset;
				const size_t transformDataIdx = unmergedBatches[unmergedSrcIdx].m_transformDataIdx;

				fr::TransformComponent::RenderData const* instanceTransformData = transformRenderData[transformDataIdx];

				// Add the Transform to our list
				instanceTransformRenderData.emplace_back(instanceTransformData);
			}
			mergedBatches.back().SetParameterBlock(gr::Mesh::CreateInstancedMeshParamsData(instanceTransformRenderData));

		} while (unmergedIdx < unmergedBatches.size());

		SEEndCPUEvent();

		return mergedBatches;
	}
}