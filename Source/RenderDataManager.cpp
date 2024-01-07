// © 2023 Adam Badke. All rights reserved.
#include "BoundsRenderData.h"
#include "Camera.h"
#include "CastUtils.h"
#include "LightRenderData.h"
#include "Material_GLTF.h"
#include "MeshPrimitive.h"
#include "RenderDataManager.h"
#include "ShadowMapRenderData.h"
#include "TransformRenderData.h"


namespace
{
	template<typename T>
	void AddIDToTrackingList(std::vector<T>& idTrackingList, T id)
	{
		SEAssert("ID has already been added to the tracking list",
			std::binary_search(idTrackingList.begin(), idTrackingList.end(), id) == false);

		// Insert in sorted order:
		idTrackingList.insert(
			std::upper_bound(idTrackingList.begin(), idTrackingList.end(), id),
			id);
	}


	template<typename T>
	void RemoveIDFromTrackingList(std::vector<T>& idTrackingList, T id)
	{
		SEAssert("ID does not exist in the tracking list",
			std::binary_search(idTrackingList.begin(), idTrackingList.end(), id) == true);

		// Remove from our sorted vector:
		auto idItr = std::equal_range(idTrackingList.begin(), idTrackingList.end(), id);
		idTrackingList.erase(idItr.first, idItr.second);
	}
}

namespace gr
{
	RenderDataManager::RenderDataManager()
		: m_currentFrame(k_invalidDirtyFrameNum)
	{
	}


	void RenderDataManager::Destroy()
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		SEAssert("An ID to data map is not empty: Was a render object not destroyed via a render command?",
			m_IDToRenderObjectMetadata.empty() && m_transformIDToTransformMetadata.empty());

		SEAssert("A registered ID list is not empty", 
			m_registeredRenderObjectIDs.empty() && m_registeredTransformIDs.empty());

		for (auto const& typeVector : m_perTypeRegisteredRenderObjectIDs)
		{
			SEAssert("A per-type registered ID list is not empty", typeVector.empty());
		}
	}


	void RenderDataManager::BeginFrame(uint64_t currentFrame)
	{
		SEAssert("Invalid frame value", 
			currentFrame != k_invalidDirtyFrameNum && 
			(m_currentFrame <= currentFrame || m_currentFrame == k_invalidDirtyFrameNum /*First frame*/));
		m_currentFrame = currentFrame;
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
				SEAssert("Received a different TransformID than what is already recorded", 
					renderObjectMetadata->second.m_transformID ==  transformID);

				renderObjectMetadata->second.m_referenceCount++;
			}
		}

		RegisterTransform(transformID);
	}


	void RenderDataManager::DestroyObject(gr::RenderDataID renderDataID)
	{
		TransformID renderObjectTransformID = gr::k_invalidTransformID;

		{
			// Catch illegal accesses during RenderData modification
			util::ScopedThreadProtector threadProjector(m_threadProtector);

			SEAssert("Trying to destroy an object that does not exist", 
				m_IDToRenderObjectMetadata.contains(renderDataID));

			RenderObjectMetadata& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);
			renderObjectTransformID = renderObjectMetadata.m_transformID;
			
			renderObjectMetadata.m_referenceCount--;
			if (renderObjectMetadata.m_referenceCount == 0)
			{
				ObjectTypeToDataIndexMap const& dataIndexMap =
					renderObjectMetadata.m_objectTypeToDataIndexMap;

				SEAssert("Cannot destroy an object with first destroying its associated data", 
					renderObjectMetadata.m_objectTypeToDataIndexMap.empty());

				m_IDToRenderObjectMetadata.erase(renderDataID);
				
				RemoveIDFromTrackingList(m_registeredRenderObjectIDs, renderDataID);
			}
		}

		UnregisterTransform(renderObjectTransformID); // Decrement the Transform ref. count, and destroy it at 0
	}


	void RenderDataManager::SetFeatureBits(gr::RenderDataID renderDataID, gr::FeatureBitmask featureBits)
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		SEAssert("Invalid object ID", m_IDToRenderObjectMetadata.contains(renderDataID));
		RenderObjectMetadata& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);

		renderObjectMetadata.m_featureBits |= featureBits;
	}


	gr::FeatureBitmask RenderDataManager::GetFeatureBits(gr::RenderDataID renderDataID) const
	{
		m_threadProtector.ValidateThreadAccess();

		SEAssert("renderDataID is not registered", m_IDToRenderObjectMetadata.contains(renderDataID));
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

		SEAssert("Trying to unregister a Transform that does not exist", 
			transformMetadataItr != m_transformIDToTransformMetadata.end());

		// Decriment our reference count. If it's zero, remove the record entirely
		transformMetadataItr->second.m_referenceCount--;

		if (transformMetadataItr->second.m_referenceCount == 0)
		{
			const DataIndex indexToMove = util::CheckedCast<DataIndex>(m_transformRenderData.size() - 1);
			const DataIndex indexToReplace = transformMetadataItr->second.m_transformIdx;

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
			
			RemoveIDFromTrackingList(m_registeredTransformIDs, transformID);
		}

		// Note: Unregistering a Transform does not dirty it as no data has changed
	}


	void RenderDataManager::SetTransformData(
		gr::TransformID transformID, gr::Transform::RenderData const& transformRenderData)
	{
		// Catch illegal accesses during RenderData modification
		util::ScopedThreadProtector threadProjector(m_threadProtector);

		auto transformMetadataItr = m_transformIDToTransformMetadata.find(transformID);

		SEAssert("Trying to set the data for a Transform that does not exist",
			transformMetadataItr != m_transformIDToTransformMetadata.end());

		const DataIndex transformDataIdx = transformMetadataItr->second.m_transformIdx;
		SEAssert("Invalid transform index", transformDataIdx < m_transformRenderData.size());

		m_transformRenderData[transformDataIdx] = transformRenderData;

		transformMetadataItr->second.m_dirtyFrame = m_currentFrame;
	}


	gr::Transform::RenderData const& RenderDataManager::GetTransformData(gr::TransformID transformID) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		auto transformMetadataItr = m_transformIDToTransformMetadata.find(transformID);

		SEAssert("Trying to get the data for a Transform that does not exist",
			transformMetadataItr != m_transformIDToTransformMetadata.end());

		const DataIndex transformDataIdx = transformMetadataItr->second.m_transformIdx;
		SEAssert("Invalid transform index", transformDataIdx < m_transformRenderData.size());

		return m_transformRenderData[transformDataIdx];
	}


	gr::Transform::RenderData const& RenderDataManager::GetTransformDataFromRenderDataID(gr::RenderDataID renderDataID) const
	{
		// Note: This function is slower than direct access via the TransformID. If you have a TransformID, use it

		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		SEAssert("Trying to find an object that does not exist", m_IDToRenderObjectMetadata.contains(renderDataID));

		RenderObjectMetadata const& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);
		
		return GetTransformData(renderObjectMetadata.m_transformID);
	}


	bool RenderDataManager::TransformIsDirty(gr::TransformID transformID) const
	{
		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		auto transformMetadataItr = m_transformIDToTransformMetadata.find(transformID);

		SEAssert("Trying to get the data for a Transform that does not exist. Are you sure you passed a TransformID?",
			transformMetadataItr != m_transformIDToTransformMetadata.end());

		SEAssert("Invalid dirty frame value",
			transformMetadataItr->second.m_dirtyFrame != k_invalidDirtyFrameNum &&
			transformMetadataItr->second.m_dirtyFrame <= m_currentFrame &&
			m_currentFrame != k_invalidDirtyFrameNum);

		return transformMetadataItr->second.m_dirtyFrame == m_currentFrame;		
	}


	bool RenderDataManager::TransformIsDirtyFromRenderDataID(gr::RenderDataID renderDataID) const
	{
		// Note: This function is slower than direct access via the TransformID. If you have a TransformID, use it

		m_threadProtector.ValidateThreadAccess(); // Any thread can get data so long as no modification is happening

		SEAssert("Trying to find an object that does not exist", m_IDToRenderObjectMetadata.contains(renderDataID));

		RenderObjectMetadata const& renderObjectMetadata = m_IDToRenderObjectMetadata.at(renderDataID);

		return TransformIsDirty(renderObjectMetadata.m_transformID);
	}


	template<typename T>
	void RenderDataManager::PopulateTypesImGuiHelper(std::vector<std::string>& names, char const* typeName) const
	{
		const DataTypeIndex dataTypeIndex = GetDataIndexFromType<T>();
		SEAssert("Index is OOB of the names array", dataTypeIndex < names.size() || 
			dataTypeIndex == k_invalidDataTypeIdx);

		if (dataTypeIndex != k_invalidDataTypeIdx)
		{
			names[dataTypeIndex] = typeName;
		}
	}


	void RenderDataManager::ShowImGuiWindow() const
	{
		static std::vector<std::string> names;
		static bool foundTypeNames = false;
		if (!foundTypeNames)
		{
			foundTypeNames = true;

			constexpr size_t k_numHardcodedNames = 8;
			names.resize(std::max(m_dataVectors.size(), k_numHardcodedNames), "Unknown");

			PopulateTypesImGuiHelper<gr::Bounds::RenderData>(names, "Bounds::RenderData");
			PopulateTypesImGuiHelper<gr::Camera::RenderData>(names, "Camera::RenderData");
			PopulateTypesImGuiHelper<gr::Light::RenderDataAmbientIBL>(names, "Light::RenderDataAmbientIBL");
			PopulateTypesImGuiHelper<gr::Light::RenderDataDirectional>(names, "Light::RenderDataDirectional");
			PopulateTypesImGuiHelper<gr::Light::RenderDataPoint>(names, "Light::RenderDataPoint");
			PopulateTypesImGuiHelper<gr::Material::RenderData>(names, "Material::RenderData");
			PopulateTypesImGuiHelper<gr::MeshPrimitive::RenderData>(names, "MeshPrimitive::RenderData");
			PopulateTypesImGuiHelper<gr::ShadowMap::RenderData>(names, "ShadowMap::RenderData");
		}
		

		ImGui::Text(std::format("Current frame: {}", m_currentFrame).c_str());
		ImGui::Text(std::format("Total data vectors: {}", m_dataVectors.size()).c_str());

		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;

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
					auto const& objectTypeToDataIdx = renderObjectMetadata.m_objectTypeToDataIndexMap.find(i);
					if (objectTypeToDataIdx == renderObjectMetadata.m_objectTypeToDataIndexMap.end())
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