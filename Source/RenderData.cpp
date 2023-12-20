// © 2023 Adam Badke. All rights reserved.
#include "Camera.h"
#include "CastUtils.h"
#include "RenderData.h"
#include "TransformComponent.h"


namespace gr
{
	RenderData::RenderData()
	{

	}


	RenderData::~RenderData()
	{

	}


	void RenderData::Destroy()
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		// ECS_CONVERSION TODO: Uncomment this once we've rearranged the shutdown order. Currently fires because the render manager is
		// being destroyed before the gameplay mgr
		/*SEAssert("Object ID to data indices map is not empty: Was a render object not destroyed via a render command?",
			m_objectIDToRenderObjectMetadata.empty());*/
	}


	void RenderData::RegisterObject(gr::RenderObjectID objectID, gr::TransformID transformID)
	{
		{
			// Catch illegal accesses during RenderData modification
			util::ScopedThreadProtector threadProjector(m_threadProtector);

			auto renderObjectMetadata = m_objectIDToRenderObjectMetadata.find(objectID);
			if (renderObjectMetadata == m_objectIDToRenderObjectMetadata.end())
			{
				m_objectIDToRenderObjectMetadata.emplace(
					objectID,
					RenderObjectMetadata{
						.m_objectTypeToDataIndexTable = ObjectTypeToDataIndexTable(),
						.m_transformID = transformID
					});
			}
			else
			{
				SEAssert("Received a different TransformID than what is already recorded", 
					renderObjectMetadata->second.m_transformID ==  transformID);

				renderObjectMetadata->second.m_referenceCount++;
			}
		}

		RegisterTransform(transformID);
	}


	void RenderData::DestroyObject(gr::RenderObjectID objectID)
	{
		TransformID renderObjectTransformID = gr::k_invalidTransformID;

		{
			// Catch illegal accesses during RenderData modification
			util::ScopedThreadProtector threadProjector(m_threadProtector);

			SEAssert("Trying to destroy an object that does not exist", m_objectIDToRenderObjectMetadata.contains(objectID));

			RenderObjectMetadata& renderObjectMetadata = m_objectIDToRenderObjectMetadata.at(objectID);

			renderObjectMetadata.m_referenceCount--;

			if (renderObjectMetadata.m_referenceCount == 0)
			{
				ObjectTypeToDataIndexTable const& dataIndexTable =
					renderObjectMetadata.m_objectTypeToDataIndexTable;

				renderObjectTransformID = renderObjectMetadata.m_transformID;

#if defined(_DEBUG)
				for (size_t dataIndexEntry = 0; dataIndexEntry < dataIndexTable.size(); dataIndexEntry++)
				{
					SEAssert("Cannot destroy an object with first destroying its associated data",
						dataIndexTable[dataIndexEntry] == k_invalidDataIdx);
				}
#endif
				m_objectIDToRenderObjectMetadata.erase(objectID);
			}
		}

		// Destroy the Transform if the refcount = 0
		if (renderObjectTransformID != gr::k_invalidTransformID)
		{
			UnregisterTransform(renderObjectTransformID);
		}
	}


	void RenderData::RegisterTransform(gr::TransformID transformID)
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		auto transformMetadataItr = m_transformIDToTransformMetadata.find(transformID);
		if (transformMetadataItr == m_transformIDToTransformMetadata.end())
		{
			const uint32_t newTransformDataIdx = util::CheckedCast<uint32_t>(m_transformRenderData.size());

			// Allocate and initialize the Transform render data
			m_transformRenderData.emplace_back();
			m_transformRenderData.back().m_transformID = transformID;

			m_transformIDToTransformMetadata.emplace(
				transformID,
				TransformMetadata{
					.m_transformIdx = newTransformDataIdx,	// Transform index
					.m_referenceCount = 1 });				// Initial reference count
		}
		else
		{
			transformMetadataItr->second.m_referenceCount++;
		}
	}


	void RenderData::UnregisterTransform(gr::TransformID transformID)
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		auto transformMetadataItr = m_transformIDToTransformMetadata.find(transformID);

		SEAssert("Trying to unregister a Transform that does not exist", 
			transformMetadataItr != m_transformIDToTransformMetadata.end());

		// Decriment our reference count. If it's zero, remove the record entirely
		transformMetadataItr->second.m_referenceCount--;

		if (transformMetadataItr->second.m_referenceCount == 0)
		{
			const size_t indexToMove = m_transformRenderData.size() - 1;
			const uint32_t indexToReplace = transformMetadataItr->second.m_transformIdx;

			SEAssert("Invalid replacement index", indexToReplace < m_transformRenderData.size());

			// Copy the transform to its new location, and remove the end element
			m_transformRenderData[indexToReplace] = m_transformRenderData[indexToMove];
			m_transformRenderData.pop_back();

			// Update the indexes stored in any records referencing the entry we just moved. Transforms can be shared,
			// so we need to check all records
			for (auto& record : m_transformIDToTransformMetadata)
			{
				if (record.second.m_transformIdx == indexToMove)
				{
					record.second.m_transformIdx = indexToReplace;
				}
			}

			// Finally, erase the TransformID record:
			m_transformIDToTransformMetadata.erase(transformID);
		}
	}


	void RenderData::SetTransformData(
		gr::TransformID transformID, fr::TransformComponent::RenderData const& transformRenderData)
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		auto transformMetadataItr = m_transformIDToTransformMetadata.find(transformID);

		SEAssert("Trying to set the data for a Transform that does not exist",
			transformMetadataItr != m_transformIDToTransformMetadata.end());

		const uint32_t transformDataIdx = transformMetadataItr->second.m_transformIdx;
		SEAssert("Invalid transform index", transformDataIdx < m_transformRenderData.size());

		m_transformRenderData[transformDataIdx] = transformRenderData;
	}


	fr::TransformComponent::RenderData const& RenderData::GetTransformData(gr::TransformID transformID) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		auto transformMetadataItr = m_transformIDToTransformMetadata.find(transformID);

		SEAssert("Trying to get the data for a Transform that does not exist",
			transformMetadataItr != m_transformIDToTransformMetadata.end());

		const uint32_t transformDataIdx = transformMetadataItr->second.m_transformIdx;
		SEAssert("Invalid transform index", transformDataIdx < m_transformRenderData.size());

		return m_transformRenderData[transformDataIdx];
	}
}