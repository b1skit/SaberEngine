// © 2024 Adam Badke. All rights reserved.
#include "Buffer.h"
#include "Light.h"
#include "LightManager.h"
#include "LightRenderData.h"
#include "RenderDataManager.h"

#include "Core/Assert.h"

#include "Core/Util/CastUtils.h"

#include "Shaders/Common/LightParams.h"


namespace
{
	template<typename T>
	LightData GetLightParamDataHelper(
		gr::RenderDataManager const& renderData,
		T const& lightRenderData,
		gr::Transform::RenderData const& transformData,
		gr::RenderDataID lightID,
		gr::Light::Type lightType)
	{
		gr::ShadowMap::RenderData const* shadowData = nullptr;
		gr::Camera::RenderData const* shadowCamData = nullptr;
		if (lightRenderData.m_hasShadow)
		{
			shadowData = &renderData.GetObjectData<gr::ShadowMap::RenderData>(lightID);
			shadowCamData = &renderData.GetObjectData<gr::Camera::RenderData>(lightID);
		}

		return gr::GetLightParamData(
			&lightRenderData,
			lightType,
			transformData,
			shadowData,
			shadowCamData);
	}
}

namespace gr
{
	void LightManager::UpdateLightBuffers(gr::RenderDataManager const& renderData)
	{
		RemoveDeletedLights(renderData);
		RegisterNewLights(renderData);
		UpdateLightBufferData(renderData);
	}


	void LightManager::RemoveDeletedLights(gr::RenderDataManager const& renderData)
	{
		auto DeleteFromMetadata = [&renderData](
			std::vector<gr::RenderDataID> const* lightIDs,
			LightMetadata& lightMetadata)
			{
				if (lightIDs == nullptr || lightIDs->empty())
				{
					return;
				}

				for (gr::RenderDataID deletedID : *lightIDs)
				{
					SEAssert(lightMetadata.m_renderDataIDToBufferIdx.contains(deletedID),
						"Trying to delete a light RenderDataID that has not been registered");

					const uint32_t deletedIdx = lightMetadata.m_renderDataIDToBufferIdx.at(deletedID);

					SEAssert(lightMetadata.m_bufferIdxToRenderDataID.contains(deletedIdx),
						"Trying to delete a light index that has not been registered");

					// Use the reverse iterator to get the details of the last entry:
					const uint32_t lastIdx = lightMetadata.m_bufferIdxToRenderDataID.rbegin()->first;
					const gr::RenderDataID lastLightID = lightMetadata.m_bufferIdxToRenderDataID.rbegin()->second;

					SEAssert(lastIdx != deletedIdx || 
						(lightMetadata.m_bufferIdxToRenderDataID.at(lastIdx) == deletedID &&
							lightMetadata.m_renderDataIDToBufferIdx.at(deletedID) == lastIdx),
						"IDs are out of sync");
					
					// Move the last entry to replace the one being deleted:
					if (lastIdx != deletedIdx)
					{
						// Record the index so we can update the buffer data it later
						lightMetadata.m_dirtyMovedIndexes.emplace_back(deletedIdx);

						// Update the metadata: The last element is moved to the deleted location
						lightMetadata.m_bufferIdxToRenderDataID.at(deletedIdx) = lastLightID;
						lightMetadata.m_renderDataIDToBufferIdx.at(lastLightID) = deletedIdx;
					}

					// Update the metadata: We remove the deleted/final element:
					lightMetadata.m_bufferIdxToRenderDataID.erase(lastIdx);
					lightMetadata.m_renderDataIDToBufferIdx.erase(deletedID);

					SEAssert(lightMetadata.m_numLights >= 1, "Removing this light will underflow the counter");
					lightMetadata.m_numLights--;
				}
			};
		DeleteFromMetadata(renderData.GetIDsWithDeletedData<gr::Light::RenderDataDirectional>(), m_directionalMetadata);
		DeleteFromMetadata(renderData.GetIDsWithDeletedData<gr::Light::RenderDataPoint>(), m_pointMetadata);
		DeleteFromMetadata(renderData.GetIDsWithDeletedData<gr::Light::RenderDataSpot>(), m_spotMetadata);
	}


	void LightManager::RegisterNewLights(gr::RenderDataManager const& renderData)
	{
		auto AddToMetadata = [](
			std::vector<gr::RenderDataID> const* lightIDs,
			LightMetadata& lightMetadata)
			{
				if (!lightIDs || lightIDs->empty())
				{
					return;
				}

				for (gr::RenderDataID newID : *lightIDs)
				{
					SEAssert(!lightMetadata.m_renderDataIDToBufferIdx.contains(newID), "Light is already registered");

					const uint32_t newLightIndex = lightMetadata.m_numLights++;
					
					lightMetadata.m_renderDataIDToBufferIdx.emplace(newID, newLightIndex);
					lightMetadata.m_bufferIdxToRenderDataID.emplace(newLightIndex, newID);

					// Note: The render data dirty IDs list also contains new object IDs, so we don't need to add new
					// objects to our dirty indexes list here
				}
			};
		AddToMetadata(renderData.GetIDsWithNewData<gr::Light::RenderDataDirectional>(), m_directionalMetadata);
		AddToMetadata(renderData.GetIDsWithNewData<gr::Light::RenderDataPoint>(), m_pointMetadata);
		AddToMetadata(renderData.GetIDsWithNewData<gr::Light::RenderDataSpot>(), m_spotMetadata);
	}


	void LightManager::UpdateLightBufferData(gr::RenderDataManager const& renderData)
	{
		auto UpdateLightBuffer = [&renderData]<typename T>(
			gr::Light::Type lightType,
			LightMetadata& lightMetadata,
			char const* bufferName)
			{
				// If the buffer does not exist we must create it:
				bool mustReallocate = lightMetadata.m_lightData == nullptr;

				if (!mustReallocate)
				{
					const uint32_t curNumBufferElements = lightMetadata.m_lightData->GetNumElements();

					// If the buffer is too small, or if the no. of lights has shrunk by too much, we must reallocate:
					mustReallocate = lightMetadata.m_numLights > 0 &&
						(lightMetadata.m_numLights > curNumBufferElements ||
							lightMetadata.m_numLights <= curNumBufferElements * k_shrinkReallocationFactor);
				}
				
				if (mustReallocate)
				{
					std::vector<LightData> lightData;
					lightData.resize(lightMetadata.m_numLights);

					// Populate the light data:
					auto lightItr = renderData.ObjectBegin<T>();
					auto const& lightItrEnd = renderData.ObjectEnd<T>();
					while (lightItr != lightItrEnd)
					{
						const gr::RenderDataID lightID = lightItr.GetRenderDataID();
						
						SEAssert(lightMetadata.m_renderDataIDToBufferIdx.contains(lightID),
							"Light ID has not been registered");

						const uint32_t lightIdx = lightMetadata.m_renderDataIDToBufferIdx.at(lightID);

						SEAssert(lightMetadata.m_bufferIdxToRenderDataID.contains(lightIdx),
							"Light index has not been registered");

						SEAssert(lightIdx < lightMetadata.m_numLights, "Light index is OOB");

						T const& lightRenderData = lightItr.Get<T>();
						gr::Transform::RenderData const& transformData = lightItr.GetTransformData();

						lightData[lightIdx] = 
							GetLightParamDataHelper(renderData, lightRenderData, transformData, lightID, lightType);

						++lightItr;
					}
					SEAssert(lightMetadata.m_numLights == lightData.size(),
						"Number of lights is out of sync with render data");


					// If there are 0 lights, create a single dummy entry so we have something to set
					if (lightData.empty())
					{
						lightData.emplace_back(LightData{});
					}


					lightMetadata.m_lightData = re::Buffer::CreateArray<LightData>(
						bufferName,
						lightData.data(),
						util::CheckedCast<uint32_t>(lightData.size()),
						re::Buffer::Mutable);
				}
				else
				{
					// Update any entries that were moved:
					std::unordered_set<gr::RenderDataID> seenIDs;

					for (uint32_t movedLightIdx : lightMetadata.m_dirtyMovedIndexes)
					{
						SEAssert(lightMetadata.m_bufferIdxToRenderDataID.contains(movedLightIdx), "Invalid light index");

						const gr::RenderDataID movedLightID = lightMetadata.m_bufferIdxToRenderDataID.at(movedLightIdx);

						T const& lightRenderData = renderData.GetObjectData<T>(movedLightID);

						gr::Transform::RenderData const& transformData =
							renderData.GetTransformDataFromRenderDataID(movedLightID);

						LightData const& lightData = 
							GetLightParamDataHelper(renderData, lightRenderData, transformData, movedLightID, lightType);

						lightMetadata.m_lightData->Commit(&lightData, movedLightIdx, 1);

						seenIDs.emplace(movedLightID);
					}

					// Note: We iterate over ALL lights (not just those that passed culling)
					auto lightItr = renderData.ObjectBegin<T>();
					auto const& lightIterEnd = renderData.ObjectEnd<T>();
					while (lightItr != lightIterEnd)
					{
						const gr::RenderDataID lightID = lightItr.GetRenderDataID();
						T const& lightRenderData = renderData.GetObjectData<T>(lightID);

						if (seenIDs.contains(lightID))
						{
							continue; // Don't double-update entries that were moved AND dirty
						}

						// Check if any of the elements related to this light are dirty:
						bool isDirty = lightItr.IsDirty<T>() || lightItr.TransformIsDirty();
						if (!isDirty && lightRenderData.m_hasShadow)
						{
							SEAssert((renderData.HasObjectData<gr::Camera::RenderData>() &&
								renderData.HasObjectData<gr::ShadowMap::RenderData>()),
								"If a light has a shadow, it must have ShadowMap::RenderData and Camera::RenderData");

							isDirty |= renderData.IsDirty<gr::Camera::RenderData>(lightID) ||
								renderData.IsDirty<gr::ShadowMap::RenderData>(lightID);
						}
						
						if (isDirty)
						{
							gr::Transform::RenderData const& transformData =
								renderData.GetTransformDataFromRenderDataID(lightID);

							LightData const& lightData =
								GetLightParamDataHelper(renderData, lightRenderData, transformData, lightID, lightType);

							SEAssert(lightMetadata.m_renderDataIDToBufferIdx.contains(lightID),
								"Light ID has not been registered");

							const uint32_t dirtyLightIdx = lightMetadata.m_renderDataIDToBufferIdx.at(lightID);

							SEAssert(dirtyLightIdx < lightMetadata.m_numLights, "Light index is OOB");

							lightMetadata.m_lightData->Commit(&lightData, dirtyLightIdx, 1);
						}

						++lightItr;
					}
				}

				// Clear the dirty indexes, regardless of whether we fully reallocated or partially updated:
				lightMetadata.m_dirtyMovedIndexes.clear();
			};

		UpdateLightBuffer.template operator()<gr::Light::RenderDataDirectional>(
			gr::Light::Directional,
			m_directionalMetadata,
			LightData::k_directionalLightDataShaderName);

		UpdateLightBuffer.template operator()<gr::Light::RenderDataPoint>(
			gr::Light::Point,
			m_pointMetadata,
			LightData::k_pointLightDataShaderName);

		UpdateLightBuffer.template operator()<gr::Light::RenderDataSpot>(
			gr::Light::Spot,
			m_spotMetadata,
			LightData::k_spotLightDataShaderName);
	}


	std::shared_ptr<re::Buffer> LightManager::GetLightDataBuffer(gr::Light::Type lightType) const
	{
		switch (lightType)
		{
		case gr::Light::Directional: return m_directionalMetadata.m_lightData;
		case gr::Light::Point: return m_pointMetadata.m_lightData;
		case gr::Light::Spot: return m_spotMetadata.m_lightData;
		case gr::Light::AmbientIBL:
		default: SEAssertF("Invalid light type");
		}
		return nullptr; // This should never happen
	}


	std::shared_ptr<re::Buffer> LightManager::GetLightIndexDataBuffer(
		gr::Light::Type lightType,
		gr::RenderDataID lightID,
		char const* shaderName) const
	{
		auto CreateSingleFrameBuffer = [&lightID, &shaderName](LightMetadata const& lightMetadata) 
			-> std::shared_ptr<re::Buffer>
			{
				SEAssert(lightMetadata.m_renderDataIDToBufferIdx.contains(lightID),
					"Light ID not registered for the given type");

				const uint32_t lightIdx = lightMetadata.m_renderDataIDToBufferIdx.at(lightID);
				SEAssert(lightIdx < lightMetadata.m_lightData->GetNumElements(), "Light index is OOB");

				return re::Buffer::Create(shaderName, GetLightIndexData(lightIdx), re::Buffer::SingleFrame);
			};

		switch (lightType)
		{
		case gr::Light::Directional: return CreateSingleFrameBuffer(m_directionalMetadata);
		case gr::Light::Point: return CreateSingleFrameBuffer(m_pointMetadata);
		case gr::Light::Spot: return CreateSingleFrameBuffer(m_spotMetadata);
		case gr::Light::AmbientIBL:
		default: SEAssertF("Invalid light type");
		}
		return nullptr; // This should never happen
	}
}