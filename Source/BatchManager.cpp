// © 2023 Adam Badke. All rights reserved.
#include "CastUtils.h"
#include "BatchManager.h"
#include "Material.h"
#include "MeshPrimitive.h"
#include "ProfilingMarkers.h"
#include "RenderDataManager.h"


namespace re
{
	std::vector<re::Batch> BatchManager::BuildBatches(gr::RenderDataManager const& renderData)
	{
		SEBeginCPUEvent("BatchManager::BuildBatches");

		struct BatchSortMetadata
		{
			re::Batch m_batch;

			gr::Transform::RenderData const* m_batchTransform;
			gr::Material::MaterialInstanceData const* m_batchMaterial;
		};

		const uint32_t expectedNumMeshPrimitives = renderData.GetNumElementsOfType<gr::MeshPrimitive::RenderData>();

		// Assemble a list of merged batches:
		std::vector<re::Batch> mergedBatches;
		std::vector<BatchSortMetadata> unmergedBatches;
		unmergedBatches.reserve(expectedNumMeshPrimitives);

		auto renderDataItr = renderData.ObjectBegin<gr::MeshPrimitive::RenderData, gr::Material::MaterialInstanceData>();
		auto const& renderDataEnd = renderData.ObjectEnd<gr::MeshPrimitive::RenderData, gr::Material::MaterialInstanceData>();
		while (renderDataItr != renderDataEnd)
		{
			gr::MeshPrimitive::RenderData const& meshPrimRenderData = renderDataItr.Get<gr::MeshPrimitive::RenderData>();
			gr::Material::MaterialInstanceData const& materialRenderData = renderDataItr.Get<gr::Material::MaterialInstanceData>();
				
			unmergedBatches.emplace_back(
				BatchSortMetadata
				{
					re::Batch(re::Batch::Lifetime::SingleFrame, meshPrimRenderData, &materialRenderData),
					&renderDataItr.GetTransformDataFromTransformID(),
					&materialRenderData
				});

			++renderDataItr;
		}


		if (!unmergedBatches.empty())
		{
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
				std::vector<gr::Transform::RenderData const*> instanceTransformRenderData;
				instanceTransformRenderData.reserve(numInstances);

				std::vector<gr::Material::MaterialInstanceData const*> instanceMaterialData;
				instanceMaterialData.reserve(numInstances);

				for (size_t instanceOffset = 0; instanceOffset < numInstances; instanceOffset++)
				{
					const size_t unmergedSrcIdx = instanceStartIdx + instanceOffset;
					
					// Add the Transform to our list
					instanceTransformRenderData.emplace_back(unmergedBatches[unmergedSrcIdx].m_batchTransform);
					instanceMaterialData.emplace_back(unmergedBatches[unmergedSrcIdx].m_batchMaterial);
				}
				
				// Set our instanced parameter blocks:
				mergedBatches.back().SetParameterBlock(gr::Transform::CreateInstancedTransformParams(
					instanceTransformRenderData));

				mergedBatches.back().SetParameterBlock(gr::Material::CreateInstancedParameterBlock(
					re::ParameterBlock::PBType::SingleFrame,
					instanceMaterialData));

			} while (unmergedIdx < unmergedBatches.size());

			SEEndCPUEvent();
		}

		return mergedBatches;
	}
}