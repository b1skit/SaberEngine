// © 2023 Adam Badke. All rights reserved.
#include "BoundsRenderData.h"
#include "Buffer.h"
#include "CameraRenderData.h"
#include "IndexedBuffer.h"
#include "LightRenderData.h"
#include "Material_GLTF.h"
#include "MeshPrimitive.h"
#include "RenderDataManager.h"
#include "ShadowMapRenderData.h"
#include "TransformRenderData.h"

#include "Core/Util/CastUtils.h"

#include "Shaders/Common/InstancingParams.h"
#include "Shaders/Common/MaterialParams.h"


namespace
{
	template<typename T>
	void AddIDToTrackingList(std::vector<T>& idTrackingList, T id)
	{
		SEAssert(std::binary_search(idTrackingList.begin(), idTrackingList.end(), id) == false,
			"ID has already been added to the tracking list");

		// Insert in sorted order:
		idTrackingList.insert(
			std::upper_bound(idTrackingList.begin(), idTrackingList.end(), id),
			id);
	}


	template<typename T>
	void RemoveIDFromTrackingList(std::vector<T>& idTrackingList, T id)
	{
		SEAssert(std::binary_search(idTrackingList.begin(), idTrackingList.end(), id) == true,
			"ID does not exist in the tracking list");

		// Remove from our sorted vector:
		auto idItr = std::equal_range(idTrackingList.begin(), idTrackingList.end(), id);
		idTrackingList.erase(idItr.first, idItr.second);
	}
}

namespace gr
{
	RenderDataManager::RenderDataManager()
		: m_currentFrame(k_invalidDirtyFrameNum)
		, m_threadProtector(true)
	{
		// Set a default identity Transform::RenderData for the gr::k_invalidTransformID:
		RegisterTransform(gr::k_invalidTransformID);
		SetTransformData(gr::k_invalidTransformID, gr::Transform::RenderData{ .m_transformID = gr::k_invalidTransformID, });

		m_indexedBufferManager = std::make_unique<gr::IndexedBufferManager>(*this);

		// Configure the indexed buffer manager:
		IndexedBufferManager::IIndexedBuffer* indexedTransforms = m_indexedBufferManager->AddIndexedBuffer(
			TransformData::s_shaderName, // Buffer name (not the shader name)
			gr::Transform::CreateInstancedTransformData,
			re::Buffer::DefaultHeap);

		indexedTransforms->AddLUTDataWriterCallback<InstanceIndexData>(InstanceIndexData::WriteTransformIndex);

		IndexedBufferManager::IIndexedBuffer* indexedMaterials = m_indexedBufferManager->AddIndexedBuffer(
			PBRMetallicRoughnessData::s_shaderName, // Buffer name (not the shader name)
			gr::Material::CreateInstancedMaterialData<PBRMetallicRoughnessData>,
			re::Buffer::DefaultHeap,
			gr::RenderObjectFeature::IsMeshPrimitiveConcept);

		indexedMaterials->AddLUTDataWriterCallback<InstanceIndexData>(InstanceIndexData::WriteMaterialIndex);
	}


	RenderDataManager::~RenderDataManager()
	{
	}


	void RenderDataManager::Destroy()
	{
		m_indexedBufferManager->Destroy();

		// Destroy the default identity Transform::RenderData for the gr::k_invalidTransformID:
		UnregisterTransform(gr::k_invalidTransformID);

		{
			// Catch illegal accesses during RenderData modification
			util::ScopedThreadProtector threadProjector(m_threadProtector);

			SEAssert(m_IDToRenderObjectMetadata.empty() && m_transformIDToTransformMetadata.empty(),
				"An ID to data map is not empty: Was a render object not destroyed via a render command?");

			SEAssert(m_registeredRenderObjectIDs.empty() && m_registeredTransformIDs.empty(),
				"A registered ID list is not empty");

			SEAssert(m_transformToRenderDataIDs.empty(),
				"TransformID -> RenderDataID multi-map not empty. This should not be possible");

			for (auto const& typeVector : m_perTypeRegisteredRenderDataIDs)
			{
				SEAssert(typeVector.empty(), "A per-type registered ID list is not empty");
			}
		}
	}


	void RenderDataManager::BeginFrame(uint64_t currentFrame)
	{
		SEAssert(currentFrame != k_invalidDirtyFrameNum && 
			(m_currentFrame <= currentFrame || m_currentFrame == k_invalidDirtyFrameNum /*First frame*/),
			"Invalid frame value");
		

		// Clear the new/deleted data ID trackers for the new frame:
		if (currentFrame != m_currentFrame)
		{
			for (auto& prevFrameNewTypes : m_perFramePerTypeNewDataIDs)
			{
				prevFrameNewTypes.clear();
			}
			for (auto& prevFrameDeletedTypes : m_perFramePerTypeDeletedDataIDs)
			{
				prevFrameDeletedTypes.clear();
			}
			m_perFrameDeletedDataIDs.clear();
			m_perFrameSeenDeletedDataIDs.clear();
			for (auto& prevFrameDirtyTypes : m_perFramePerTypeDirtyDataIDs)
			{
				prevFrameDirtyTypes.clear();
			}
			for (auto& prevFrameDirtyTypeIDs : m_perFramePerTypeDirtySeenDataIDs)
			{
				prevFrameDirtyTypeIDs.clear();
			}

			// Transforms:
			m_perFrameNewTransformIDs.clear();
			m_perFrameDeletedTransformIDs.clear();
			m_perFrameDirtyTransformIDs.clear();
			m_perFrameSeenDirtyTransformIDs.clear();
		}

		m_currentFrame = currentFrame;
	}


	void RenderDataManager::Update()
	{
		m_indexedBufferManager->Update(); // Must be called after render data has been populated for the current frame
	}


	gr::IndexedBufferManager& RenderDataManager::GetInstancingIndexedBufferManager() const
	{
		return *m_indexedBufferManager;
	}


	void RenderDataManager::RegisterObject(gr::RenderDataID renderDataID, gr::TransformID transformID)
	{
		{
			// Catch illegal accesses during RenderData modification
			util::ScopedThreadProtector threadProjector(m_threadProtector);

			auto renderObjectMetadata = m_IDToRenderObjectMetadata.find(renderDataID);
			if (renderObjectMetadata == m_IDToRenderObjectMetadata.end())
			{
				m_IDToRenderObjectMetadata.emplace(
					renderDataID,
					RenderObjectMetadata(transformID));

				AddIDToTrackingList(m_registeredRenderObjectIDs, renderDataID);
			}
			else
			{
				SEAssert(renderObjectMetadata->second.m_transformID ==  transformID,
					"Received a different TransformID than what is already recorded");

				renderObjectMetadata->second.m_referenceCount++;
			}

			// Multi-map our TransformID -> RenderDataID:
			m_transformToRenderDataIDs.emplace(transformID, renderDataID);
		}

		RegisterTransform(transformID);
	}


	void RenderDataManager::DestroyObject(gr::RenderDataID renderDataID)
	{
		TransformID transformID = gr::k_invalidTransformID;

		{
			// Catch illegal accesses during RenderData modification
			util::ScopedThreadProtector threadProjector(m_threadProtector);

			SEAssert(m_IDToRenderObjectMetadata.contains(renderDataID),
				"Trying to destroy an object that does not exist");

			RenderObjectMetadata& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);
			transformID = renderObjectMetadata.m_transformID;

			renderObjectMetadata.m_referenceCount--;
			if (renderObjectMetadata.m_referenceCount == 0)
			{
				ObjectTypeToDataIndexMap const& dataIndexMap =
					renderObjectMetadata.m_dataTypeToDataIndexMap;

				SEAssert(renderObjectMetadata.m_dataTypeToDataIndexMap.empty(),
					"Cannot destroy an object with first destroying its associated data");

				m_IDToRenderObjectMetadata.erase(renderDataID);
				
				RemoveIDFromTrackingList(m_registeredRenderObjectIDs, renderDataID);

				// Remove the RenderDataID from our TransformID -> RenderDataID multi-map:
				auto transformToRenderDataIDsItr = m_transformToRenderDataIDs.equal_range(transformID);
				auto curItr = transformToRenderDataIDsItr.first;
				while (curItr != transformToRenderDataIDsItr.second)
				{
					if (curItr->second == renderDataID)
					{
						m_transformToRenderDataIDs.erase(curItr);
						break;
					}
					++curItr;
				}
			}
		}

		UnregisterTransform(transformID); // Decrement the Transform ref. count, and destroy it at 0
	}


	void RenderDataManager::SetFeatureBits(gr::RenderDataID renderDataID, gr::FeatureBitmask featureBits)
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		SEAssert(m_IDToRenderObjectMetadata.contains(renderDataID), "Invalid object ID");
		RenderObjectMetadata& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);

		renderObjectMetadata.m_featureBits |= featureBits;
	}


	gr::FeatureBitmask RenderDataManager::GetFeatureBits(gr::RenderDataID renderDataID) const
	{
		m_threadProtector.ValidateThreadAccess();

		SEAssert(m_IDToRenderObjectMetadata.contains(renderDataID), "renderDataID is not registered");
		RenderObjectMetadata const& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);

		return renderObjectMetadata.m_featureBits;
	}


	void RenderDataManager::RegisterTransform(gr::TransformID transformID)
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		auto transformMetadataItr = m_transformIDToTransformMetadata.find(transformID);
		if (transformMetadataItr == m_transformIDToTransformMetadata.end())
		{
			const DataIndex newTransformDataIdx = util::CheckedCast<DataIndex>(m_transformRenderData.size());

			// Allocate and initialize the Transform render data
			m_transformRenderData.emplace_back();
			m_transformRenderData.back().m_transformID = transformID;

			m_transformIDToTransformMetadata.emplace(
				transformID,
				TransformMetadata{
					.m_transformIdx = newTransformDataIdx,	// Transform index
					.m_referenceCount = 1,					// Initial reference count
					.m_dirtyFrame = m_currentFrame});

			AddIDToTrackingList(m_registeredTransformIDs, transformID);
			AddIDToTrackingList(m_perFrameNewTransformIDs, transformID);
		}
		else
		{
			transformMetadataItr->second.m_referenceCount++;
		}
	}


	void RenderDataManager::UnregisterTransform(gr::TransformID transformID)
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		auto transformMetadataItr = m_transformIDToTransformMetadata.find(transformID);

		SEAssert(transformMetadataItr != m_transformIDToTransformMetadata.end(),
			"Trying to unregister a Transform that does not exist");

		// Decriment our reference count. If it's zero, remove the record entirely
		transformMetadataItr->second.m_referenceCount--;

		SEAssert(transformMetadataItr->second.m_referenceCount == m_transformToRenderDataIDs.count(transformID) ||
			(transformID == k_invalidTransformID && 
				transformMetadataItr->second.m_referenceCount == m_transformToRenderDataIDs.count(transformID) + 1),
			"TransformID to RenderDataID map is out of sync");

		if (transformMetadataItr->second.m_referenceCount == 0)
		{
			const DataIndex indexToMove = util::CheckedCast<DataIndex>(m_transformRenderData.size() - 1);
			const DataIndex indexToReplace = transformMetadataItr->second.m_transformIdx;

			SEAssert(indexToReplace < m_transformRenderData.size(), "Invalid replacement index");

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
			
			RemoveIDFromTrackingList(m_registeredTransformIDs, transformID);
			AddIDToTrackingList(m_perFrameDeletedTransformIDs, transformID);
		}

		// Note: Unregistering a Transform does not dirty it as no data has changed
	}


	void RenderDataManager::SetTransformData(
		gr::TransformID transformID, gr::Transform::RenderData const& transformRenderData)
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		auto transformMetadataItr = m_transformIDToTransformMetadata.find(transformID);

		SEAssert(transformMetadataItr != m_transformIDToTransformMetadata.end(),
			"Trying to set the data for a Transform that does not exist");

		const DataIndex transformDataIdx = transformMetadataItr->second.m_transformIdx;
		SEAssert(transformDataIdx < m_transformRenderData.size(), "Invalid transform index");

		m_transformRenderData[transformDataIdx] = transformRenderData;

		transformMetadataItr->second.m_dirtyFrame = m_currentFrame;

		// If this is the first time we've modified the transform this frame, add the TransformID to our tracking table
		if (m_perFrameSeenDirtyTransformIDs.emplace(transformID).second)
		{
			m_perFrameDirtyTransformIDs.emplace_back(transformID);
		}
	}


	gr::Transform::RenderData const& RenderDataManager::GetTransformDataFromTransformID(gr::TransformID transformID) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		auto transformMetadataItr = m_transformIDToTransformMetadata.find(transformID);

		SEAssert(transformMetadataItr != m_transformIDToTransformMetadata.end(),
			"Trying to get the data for a Transform that does not exist");

		const DataIndex transformDataIdx = transformMetadataItr->second.m_transformIdx;
		SEAssert(transformDataIdx < m_transformRenderData.size(), "Invalid transform index");

		return m_transformRenderData[transformDataIdx];
	}


	gr::Transform::RenderData const& RenderDataManager::GetTransformDataFromRenderDataID(gr::RenderDataID renderDataID) const
	{
		// Note: This function is slower than direct access via the TransformID. If you have a TransformID, use it

		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		SEAssert(m_IDToRenderObjectMetadata.contains(renderDataID), "Trying to find an object that does not exist");

		RenderObjectMetadata const& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);
		
		return GetTransformDataFromTransformID(renderObjectMetadata.m_transformID);
	}


	bool RenderDataManager::TransformIsDirty(gr::TransformID transformID) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		if (transformID == gr::k_invalidTransformID)
		{
			return false; // The default identity transform is never dirty
		}

		auto transformMetadataItr = m_transformIDToTransformMetadata.find(transformID);

		SEAssert(transformMetadataItr != m_transformIDToTransformMetadata.end(),
			"Trying to get the data for a Transform that does not exist. Are you sure you passed a TransformID?");

		SEAssert(transformMetadataItr->second.m_dirtyFrame != k_invalidDirtyFrameNum &&
			transformMetadataItr->second.m_dirtyFrame <= m_currentFrame &&
			m_currentFrame != k_invalidDirtyFrameNum,
			"Invalid dirty frame value");

		return transformMetadataItr->second.m_dirtyFrame == m_currentFrame;		
	}


	bool RenderDataManager::TransformIsDirtyFromRenderDataID(gr::RenderDataID renderDataID) const
	{
		// Note: This function is slower than direct access via the TransformID. If you have a TransformID, use it

		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		SEAssert(m_IDToRenderObjectMetadata.contains(renderDataID), "Trying to find an object that does not exist");

		RenderObjectMetadata const& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);

		return TransformIsDirty(renderObjectMetadata.m_transformID);
	}


	std::vector<gr::TransformID> const& RenderDataManager::GetIDsWithDirtyTransformData() const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		return m_perFrameDirtyTransformIDs;
	}


	std::vector<gr::TransformID> const& RenderDataManager::GetNewTransformIDs() const
	{
		return m_perFrameNewTransformIDs;
	}


	std::vector<gr::TransformID> const& RenderDataManager::GetDeletedTransformIDs() const
	{
		return m_perFrameDeletedTransformIDs;
	}


	template<typename T>
	void RenderDataManager::PopulateTypesImGuiHelper(std::vector<std::string>& names, char const* typeName) const
	{
		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		SEAssert(dataTypeIndex < names.size() || dataTypeIndex == k_invalidDataTypeIdx,
			"Index is OOB of the names array");

		if (dataTypeIndex != k_invalidDataTypeIdx)
		{
			names[dataTypeIndex] = typeName;
		}
	}


	void RenderDataManager::ShowImGuiWindow() const
	{
		constexpr size_t k_numHardcodedNames = 11;
		std::vector<std::string> names(std::max(m_dataVectors.size(), k_numHardcodedNames), "Unknown");

		PopulateTypesImGuiHelper<gr::Bounds::RenderData>(names, "Bounds::RenderData");
		PopulateTypesImGuiHelper<gr::Camera::RenderData>(names, "Camera::RenderData");
		PopulateTypesImGuiHelper<gr::Light::RenderDataAmbientIBL>(names, "Light::RenderDataAmbientIBL");
		PopulateTypesImGuiHelper<gr::Light::RenderDataDirectional>(names, "Light::RenderDataDirectional");
		PopulateTypesImGuiHelper<gr::Light::RenderDataPoint>(names, "Light::RenderDataPoint");
		PopulateTypesImGuiHelper<gr::Light::RenderDataSpot>(names, "Light::RenderDataSpot");
		PopulateTypesImGuiHelper<gr::Material::MaterialInstanceRenderData>(names, "Material::MaterialInstanceRenderData");
		PopulateTypesImGuiHelper<gr::MeshPrimitive::RenderData>(names, "MeshPrimitive::RenderData");
		PopulateTypesImGuiHelper<gr::MeshPrimitive::MeshMorphRenderData>(names, "MeshPrimitive::MeshMorphRenderData");
		PopulateTypesImGuiHelper<gr::MeshPrimitive::SkinningRenderData>(names, "MeshPrimitive::SkinningRenderData");
		PopulateTypesImGuiHelper<gr::ShadowMap::RenderData>(names, "ShadowMap::RenderData");
		

		ImGui::Text(std::format("Current frame: {}", m_currentFrame).c_str());
		ImGui::Text(std::format("Total data vectors: {}", m_dataVectors.size()).c_str());

		constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;

		const DataTypeIndex numDataTypes = util::CheckedCast<DataTypeIndex>(m_dataVectors.size());
		const int numCols = numDataTypes + 3;
		if (ImGui::BeginTable("m_IDToRenderObjectMetadata", numCols, flags))
		{
			// Headers:				
			ImGui::TableSetupColumn("RenderObjectID (ref. count)");
			ImGui::TableSetupColumn("TransformID (ref.count) [dirty frame]");
			ImGui::TableSetupColumn("Feature bits");
			for (DataTypeIndex i = 0; i < numDataTypes; i++)
			{
				ImGui::TableSetupColumn(std::format("{}: {} [dirty frame]", i, names[i]).c_str());
			}
			ImGui::TableHeadersRow();


			for (auto const& entry : m_IDToRenderObjectMetadata)
			{
				const gr::RenderDataID renderDataID = entry.first;
				RenderObjectMetadata const& renderObjectMetadata = entry.second;

				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				// RenderDataID (Ref. count)
				ImGui::Text(std::format("{} ({})", renderDataID, renderObjectMetadata.m_referenceCount).c_str());

				ImGui::TableNextColumn();				

				// TransformID (Ref. count) [dirty frame]
				ImGui::Text(std::format("{} ({}) [{}]",
					renderObjectMetadata.m_transformID,
					m_transformIDToTransformMetadata.at(renderObjectMetadata.m_transformID).m_referenceCount,
					m_transformIDToTransformMetadata.at(renderObjectMetadata.m_transformID).m_dirtyFrame).c_str());

				ImGui::TableNextColumn();

				// Feature bits
				ImGui::Text(std::format("{:b}", renderObjectMetadata.m_featureBits).c_str());

				for (DataTypeIndex i = 0; i < numDataTypes; i++)
				{
					ImGui::TableNextColumn();

					std::string cellText;

					// ObjectTypeToDataIndexMap
					auto const& objectTypeToDataIdx = renderObjectMetadata.m_dataTypeToDataIndexMap.find(i);
					if (objectTypeToDataIdx == renderObjectMetadata.m_dataTypeToDataIndexMap.end())
					{
						cellText = "-";
					}
					else
					{
						
						cellText = std::format("{}", objectTypeToDataIdx->second).c_str();
					}

					cellText += " ";

					// LastDirtyFrameMap
					auto const& dirtyFrameRecord = renderObjectMetadata.m_dirtyFrameMap.find(i);
					if (dirtyFrameRecord == renderObjectMetadata.m_dirtyFrameMap.end())
					{
						cellText += "[-]";
					}
					else
					{
						cellText += std::format("[{}]", renderObjectMetadata.m_dirtyFrameMap.at(i)).c_str();
					}

					ImGui::Text(cellText.c_str());
				}
			}

			ImGui::EndTable();
		}
	}
}