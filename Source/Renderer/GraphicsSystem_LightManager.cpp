// © 2024 Adam Badke. All rights reserved.
#include "GraphicsSystem_LightManager.h"
#include "GraphicsSystemManager.h"
#include "LightParamsHelpers.h"
#include "RenderDataManager.h"

#include "Core/Config.h"

#include "Shaders/Common/LightParams.h"


namespace
{
	template<typename T>
	LightData GetLightParamDataHelper(
		gr::RenderDataManager const& renderData,
		T const& lightRenderData,
		gr::Transform::RenderData const& transformData,
		gr::RenderDataID lightID,
		gr::Light::Type lightType,
		core::InvPtr<re::Texture> const& shadowTex,
		uint32_t shadowArrayIdx)
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
			shadowCamData,
			shadowTex,
			shadowArrayIdx);
	}
}

namespace gr
{
	LightManagerGraphicsSystem::LightManagerGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
	{
	}


	void LightManagerGraphicsSystem::RegisterInputs()
	{
		//
	}


	void LightManagerGraphicsSystem::RegisterOutputs()
	{
		// Monolithic light buffers:
		RegisterBufferOutput(k_directionalLightDataBufferOutput, &m_directionalLightMetadata.m_lightData);
		RegisterBufferOutput(k_pointLightDataBufferOutput, &m_pointLightMetadata.m_lightData);
		RegisterBufferOutput(k_spotLightDataBufferOutput, &m_spotLightMetadata.m_lightData);

		// RenderDataID -> monolithic light buffer index maps
		RegisterDataOutput(k_IDToDirectionalIdxDataOutput, &m_directionalLightMetadata.m_renderDataIDToBufferIdx);
		RegisterDataOutput(k_IDToPointIdxDataOutput, &m_pointLightMetadata.m_renderDataIDToBufferIdx);
		RegisterDataOutput(k_IDToSpotIdxDataOutput, &m_spotLightMetadata.m_renderDataIDToBufferIdx);

		// Shadow array textures:
		RegisterTextureOutput(k_directionalShadowArrayTexOutput, &m_directionalShadowMetadata.m_shadowArray);
		RegisterTextureOutput(k_pointShadowArrayTexOutput, &m_pointShadowMetadata.m_shadowArray);
		RegisterTextureOutput(k_spotShadowArrayTexOutput, &m_spotShadowMetadata.m_shadowArray);

		// RenderDataID -> shadow texture array index maps
		RegisterDataOutput(k_IDToDirectionalShadowArrayIdxDataOutput, &m_directionalShadowMetadata.m_renderDataIDToTexArrayIdx);
		RegisterDataOutput(k_IDToPointShadowArrayIdxDataOutput, &m_pointShadowMetadata.m_renderDataIDToTexArrayIdx);
		RegisterDataOutput(k_IDToSpotShadowArrayIdxDataOutput, &m_spotShadowMetadata.m_renderDataIDToTexArrayIdx);

		RegisterBufferOutput(k_PCSSSampleParamsBufferOutput, &m_poissonSampleParamsBuffer);
	}


	void LightManagerGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline,
		TextureDependencies const& texDependencies,
		BufferDependencies const& bufferDependencies,
		DataDependencies const&)
	{
		PoissonSampleParamsData const& poissonSampleParamsData = GetPoissonSampleParamsData();

		m_poissonSampleParamsBuffer = re::Buffer::Create(
			PoissonSampleParamsData::s_shaderName,
			poissonSampleParamsData,
			re::Buffer::BufferParams{
				.m_stagingPool = re::Buffer::StagingPool::Temporary,
				.m_memPoolPreference = re::Buffer::UploadHeap,
				.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
				.m_usageMask = re::Buffer::Constant,
			});
	}


	void LightManagerGraphicsSystem::PreRender()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		RemoveDeletedLights(renderData);
		RegisterNewLights(renderData);
		UpdateLightBufferData(renderData);
	}


	uint32_t LightManagerGraphicsSystem::GetShadowArrayIndex(
		ShadowMetadata const& shadowMetadata, gr::RenderDataID lightID) const
	{
		uint32_t shadowArrayIdx = INVALID_SHADOW_IDX;
		if (shadowMetadata.m_renderDataIDToTexArrayIdx.contains(lightID))
		{
			shadowArrayIdx = shadowMetadata.m_renderDataIDToTexArrayIdx.at(lightID);
		}
		return shadowArrayIdx;
	};


	uint32_t LightManagerGraphicsSystem::GetShadowArrayIndex(gr::Light::Type lightType, gr::RenderDataID lightID) const
	{
		switch (lightType)
		{
		case gr::Light::Directional:
		{
			return GetShadowArrayIndex(m_directionalShadowMetadata, lightID);
		}
		break;
		case gr::Light::Point:
		{
			return GetShadowArrayIndex(m_pointShadowMetadata, lightID);
		}
		break;
		case gr::Light::Spot:
		{
			return GetShadowArrayIndex(m_spotShadowMetadata, lightID);
		}
		break;
		case gr::Light::AmbientIBL:
		default: SEAssertF("Invalid light type");
		}
		return 0; // This should never happen
	}


	void LightManagerGraphicsSystem::RemoveDeletedLights(gr::RenderDataManager const& renderData)
	{
		auto DeleteLightMetadata = [](
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
		DeleteLightMetadata(renderData.GetIDsWithDeletedData<gr::Light::RenderDataDirectional>(), m_directionalLightMetadata);
		DeleteLightMetadata(renderData.GetIDsWithDeletedData<gr::Light::RenderDataPoint>(), m_pointLightMetadata);
		DeleteLightMetadata(renderData.GetIDsWithDeletedData<gr::Light::RenderDataSpot>(), m_spotLightMetadata);

		std::vector<gr::RenderDataID> const* deletedShadows =
			renderData.GetIDsWithDeletedData<gr::ShadowMap::RenderData>();
		if (deletedShadows && !deletedShadows->empty())
		{
			auto shadowItr = renderData.IDBegin(*deletedShadows);
			auto const& shadowItrEnd = renderData.IDEnd(*deletedShadows);
			while (shadowItr != shadowItrEnd)
			{
				const gr::RenderDataID deletedID = shadowItr.GetRenderDataID();

				bool foundShadow = false;

				auto DeleteShadowEntry = [&deletedID, &foundShadow](ShadowMetadata& shadowMetadata)
					{
						if (!shadowMetadata.m_renderDataIDToTexArrayIdx.contains(deletedID))
						{
							return;
						}
						foundShadow = true;

						const uint32_t deletedIdx = shadowMetadata.m_renderDataIDToTexArrayIdx.at(deletedID);

						SEAssert(shadowMetadata.m_texArrayIdxToRenderDataID.contains(deletedIdx),
							"Trying to delete a light index that has not been registered");

						// Use the reverse iterator to get the details of the last entry:
						const uint32_t lastIdx = shadowMetadata.m_texArrayIdxToRenderDataID.rbegin()->first;
						const gr::RenderDataID lastLightID = shadowMetadata.m_texArrayIdxToRenderDataID.rbegin()->second;

						SEAssert(lastIdx != deletedIdx ||
							(shadowMetadata.m_texArrayIdxToRenderDataID.at(lastIdx) == deletedID &&
								shadowMetadata.m_renderDataIDToTexArrayIdx.at(deletedID) == lastIdx),
							"IDs are out of sync");

						// Move the last entry to replace the one being deleted:
						if (lastIdx != deletedIdx)
						{
							// Update the metadata: The last element is moved to the deleted location
							shadowMetadata.m_texArrayIdxToRenderDataID.at(deletedIdx) = lastLightID;
							shadowMetadata.m_renderDataIDToTexArrayIdx.at(lastLightID) = deletedIdx;
						}

						// Update the metadata: We remove the deleted/final element:
						shadowMetadata.m_texArrayIdxToRenderDataID.erase(lastIdx);
						shadowMetadata.m_renderDataIDToTexArrayIdx.erase(deletedID);

						SEAssert(shadowMetadata.m_numShadows >= 1, "Removing this light will underflow the counter");
						shadowMetadata.m_numShadows--;
					};
				// Try to delete in order of most expected lights to least:
				DeleteShadowEntry(m_pointShadowMetadata);
				if (!foundShadow)
				{
					DeleteShadowEntry(m_spotShadowMetadata);
				}
				if (!foundShadow)
				{
					DeleteShadowEntry(m_directionalShadowMetadata);
				}
				SEAssert(foundShadow, "Trying to delete a light RenderDataID that has not been registered");

				++shadowItr;
			}
		}
	}


	void LightManagerGraphicsSystem::RegisterNewLights(gr::RenderDataManager const& renderData)
	{
		auto AddToLightMetadata = [](
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

					SEAssert(lightMetadata.m_renderDataIDToBufferIdx.size() == lightMetadata.m_numLights &&
						lightMetadata.m_bufferIdxToRenderDataID.size() == lightMetadata.m_numLights,
						"Number of lights is out of sync");

					// Note: The render data dirty IDs list also contains new object IDs, so we don't need to add new
					// objects to our dirty indexes list here
				}
			};
		AddToLightMetadata(renderData.GetIDsWithNewData<gr::Light::RenderDataDirectional>(), m_directionalLightMetadata);
		AddToLightMetadata(renderData.GetIDsWithNewData<gr::Light::RenderDataPoint>(), m_pointLightMetadata);
		AddToLightMetadata(renderData.GetIDsWithNewData<gr::Light::RenderDataSpot>(), m_spotLightMetadata);


		std::vector<gr::RenderDataID> const* newShadows = renderData.GetIDsWithNewData<gr::ShadowMap::RenderData>();
		if (newShadows && !newShadows->empty())
		{
			auto shadowItr = renderData.IDBegin(*newShadows);
			auto const& shadowItrEnd = renderData.IDEnd(*newShadows);
			while (shadowItr != shadowItrEnd)
			{
				const gr::RenderDataID shadowID = shadowItr.GetRenderDataID();

				gr::ShadowMap::RenderData const& shadowMapRenderData = shadowItr.Get<gr::ShadowMap::RenderData>();

				auto AddShadowToMetadata = [&shadowID](ShadowMetadata& shadowMetadata)
					{
						SEAssert(!shadowMetadata.m_renderDataIDToTexArrayIdx.contains(shadowID),
							"Shadow is already registered");

						const uint32_t newShadowIndex = shadowMetadata.m_numShadows++;

						shadowMetadata.m_renderDataIDToTexArrayIdx.emplace(shadowID, newShadowIndex);
						shadowMetadata.m_texArrayIdxToRenderDataID.emplace(newShadowIndex, shadowID);

						SEAssert(shadowMetadata.m_renderDataIDToTexArrayIdx.size() == shadowMetadata.m_numShadows &&
							shadowMetadata.m_texArrayIdxToRenderDataID.size() == shadowMetadata.m_numShadows,
							"Number of shadows counter is out of sync");

						// Note: The render data dirty IDs list also contains new object IDs, so we don't need to add new
						// objects to our dirty indexes list here
					};

				switch (shadowMapRenderData.m_lightType)
				{
				case gr::Light::Type::Directional: AddShadowToMetadata(m_directionalShadowMetadata); break;
				case gr::Light::Type::Point: AddShadowToMetadata(m_pointShadowMetadata); break;
				case gr::Light::Type::Spot: AddShadowToMetadata(m_spotShadowMetadata); break;
				case gr::Light::Type::AmbientIBL:
				default: SEAssertF("Invalid light type");
				}

				++shadowItr;
			}
		}
	}


	void LightManagerGraphicsSystem::UpdateLightBufferData(gr::RenderDataManager const& renderData)
	{
		// We update the shadows first, as we pack some shadow texture parameters into the LightData buffer
		auto UpdateShadowTexture = [](
			gr::Light::Type lightType,
			ShadowMetadata& shadowMetadata,
			char const* shadowTexName)
			{
				// If the buffer does not exist we must create it:
				bool mustReallocate = shadowMetadata.m_shadowArray == nullptr;

				if (!mustReallocate)
				{
					const uint32_t curNumTexArrayElements = shadowMetadata.m_shadowArray->GetTextureParams().m_arraySize;

					// If the buffer is too small, or if the no. of lights has shrunk by too much, we must reallocate:
					mustReallocate = shadowMetadata.m_numShadows > 0 &&
						(shadowMetadata.m_numShadows > curNumTexArrayElements ||
							shadowMetadata.m_numShadows <= curNumTexArrayElements * k_shrinkReallocationFactor);
				}

				if (mustReallocate)
				{
					re::Texture::TextureParams shadowArrayParams;

					switch (lightType)
					{
					case gr::Light::Directional:
					{
						const int defaultDirectionalWidthHeight =
							core::Config::Get()->GetValue<int>(core::configkeys::k_defaultDirectionalShadowMapResolutionKey);

						shadowArrayParams.m_width = defaultDirectionalWidthHeight;
						shadowArrayParams.m_height = defaultDirectionalWidthHeight;
						shadowArrayParams.m_dimension = re::Texture::Dimension::Texture2DArray;
					}
					break;
					case gr::Light::Point:
					{
						const int defaultCubemapWidthHeight =
							core::Config::Get()->GetValue<int>(core::configkeys::k_defaultShadowCubeMapResolutionKey);

						shadowArrayParams.m_width = defaultCubemapWidthHeight;
						shadowArrayParams.m_height = defaultCubemapWidthHeight;
						shadowArrayParams.m_dimension = re::Texture::Dimension::TextureCubeArray;
					}
					break;
					case gr::Light::Spot:
					{
						const int defaultSpotWidthHeight =
							core::Config::Get()->GetValue<int>(core::configkeys::k_defaultSpotShadowMapResolutionKey);

						shadowArrayParams.m_width = defaultSpotWidthHeight;
						shadowArrayParams.m_height = defaultSpotWidthHeight;
						shadowArrayParams.m_dimension = re::Texture::Dimension::Texture2DArray;
					}
					break;
					case gr::Light::AmbientIBL:
					default: SEAssertF("Invalid light type");
					}

					shadowArrayParams.m_arraySize = std::max(1u, shadowMetadata.m_numShadows);

					shadowArrayParams.m_usage =
						static_cast<re::Texture::Usage>(re::Texture::Usage::DepthTarget | re::Texture::Usage::ColorSrc);

					shadowArrayParams.m_format = re::Texture::Format::Depth32F;
					shadowArrayParams.m_colorSpace = re::Texture::ColorSpace::Linear;
					shadowArrayParams.m_mipMode = re::Texture::MipMode::None;

					shadowArrayParams.m_clear.m_depthStencil.m_depth = 1.f;

					shadowMetadata.m_shadowArray = re::Texture::Create(shadowTexName, shadowArrayParams);
				}
			};
		UpdateShadowTexture(gr::Light::Directional, m_directionalShadowMetadata, "Directional shadows");
		UpdateShadowTexture(gr::Light::Point, m_pointShadowMetadata, "Point shadows");
		UpdateShadowTexture(gr::Light::Spot, m_spotShadowMetadata, "Spot shadows");


		auto UpdateLightBuffer = [&renderData, this]<typename T>(
			gr::Light::Type lightType,
			LightMetadata & lightMetadata,
			ShadowMetadata const& shadowMetadata,
			char const* bufferName)
		{
			// If the buffer does not exist we must create it:
			bool mustReallocate = lightMetadata.m_lightData == nullptr;

			if (!mustReallocate)
			{
				const uint32_t curNumBufferElements = lightMetadata.m_lightData->GetArraySize();

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

					const uint32_t shadowArrayIdx = GetShadowArrayIndex(shadowMetadata, lightID);

					SEAssert(lightMetadata.m_bufferIdxToRenderDataID.contains(lightIdx),
						"Light index has not been registered");

					SEAssert(lightIdx < lightMetadata.m_numLights, "Light index is OOB");

					T const& lightRenderData = lightItr.Get<T>();
					gr::Transform::RenderData const& transformData = lightItr.GetTransformData();

					lightData[lightIdx] = GetLightParamDataHelper(
						renderData,
						lightRenderData,
						transformData,
						lightID,
						lightType,
						shadowMetadata.m_shadowArray,
						shadowArrayIdx);

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
					re::Buffer::BufferParams{
						.m_stagingPool = re::Buffer::StagingPool::Permanent,
						.m_memPoolPreference = re::Buffer::UploadHeap,
						.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
						.m_usageMask = re::Buffer::Structured,
						.m_arraySize = util::CheckedCast<uint32_t>(lightData.size()),
					});
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

					const uint32_t shadowArrayIdx = GetShadowArrayIndex(shadowMetadata, movedLightID);

					LightData const& lightData = GetLightParamDataHelper(
						renderData,
						lightRenderData,
						transformData,
						movedLightID,
						lightType,
						shadowMetadata.m_shadowArray,
						shadowArrayIdx);

					lightMetadata.m_lightData->Commit(&lightData, movedLightIdx, 1);

					seenIDs.emplace(movedLightID);
				}

				// Note: We iterate over ALL lights (not just those that passed culling)
				auto lightItr = renderData.ObjectBegin<T>();
				auto const& lightIterEnd = renderData.ObjectEnd<T>();
				while (lightItr != lightIterEnd)
				{
					const gr::RenderDataID lightID = lightItr.GetRenderDataID();

					if (!seenIDs.contains(lightID))// Don't double-update entries that were moved AND dirty
					{
						T const& lightRenderData = renderData.GetObjectData<T>(lightID);

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

							const uint32_t shadowArrayIdx = GetShadowArrayIndex(shadowMetadata, lightID);

							LightData const& lightData = GetLightParamDataHelper(
								renderData,
								lightRenderData,
								transformData,
								lightID,
								lightType,
								shadowMetadata.m_shadowArray,
								shadowArrayIdx);

							SEAssert(lightMetadata.m_renderDataIDToBufferIdx.contains(lightID),
								"Light ID has not been registered");

							const uint32_t dirtyLightIdx = lightMetadata.m_renderDataIDToBufferIdx.at(lightID);

							SEAssert(dirtyLightIdx < lightMetadata.m_numLights, "Light index is OOB");

							lightMetadata.m_lightData->Commit(&lightData, dirtyLightIdx, 1);
						}
					}

					++lightItr;
				}
			}

			// Clear the dirty indexes, regardless of whether we fully reallocated or partially updated:
			lightMetadata.m_dirtyMovedIndexes.clear();
		};

		UpdateLightBuffer.template operator() < gr::Light::RenderDataDirectional > (
			gr::Light::Directional,
			m_directionalLightMetadata,
			m_directionalShadowMetadata,
			LightData::s_directionalLightDataShaderName);

		UpdateLightBuffer.template operator() < gr::Light::RenderDataPoint > (
			gr::Light::Point,
			m_pointLightMetadata,
			m_pointShadowMetadata,
			LightData::s_pointLightDataShaderName);

		UpdateLightBuffer.template operator() < gr::Light::RenderDataSpot > (
			gr::Light::Spot,
			m_spotLightMetadata,
			m_spotShadowMetadata,
			LightData::s_spotLightDataShaderName);
	}


	void LightManagerGraphicsSystem::ShowImGuiWindow()
	{
		constexpr ImGuiTableFlags k_tableFlags =
			ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;

		auto ShowLightMetadata = [&k_tableFlags](LightMetadata const& lightMetadata)
			{
				ImGui::Indent();
				ImGui::Text(std::format("No. of lights: {}", lightMetadata.m_numLights).c_str());
				ImGui::Text(std::format("LightData Buffer size{}: {}",
					lightMetadata.m_numLights == 0 ? " (including dummy)" : "",
					lightMetadata.m_lightData->GetArraySize()).c_str());
				ImGui::Unindent();
			};

		auto ShowShadowMetadata = [](ShadowMetadata const& shadowMetadata)
			{
				ImGui::Indent();
				ImGui::Text(std::format("No. of shadows: {}", shadowMetadata.m_numShadows).c_str());
				ImGui::Text(std::format("Shadow array size: {}",
					shadowMetadata.m_shadowArray->GetTextureParams().m_arraySize).c_str());
				ImGui::Text(std::format("Shadow array element width: {}",
					shadowMetadata.m_shadowArray->GetTextureParams().m_width).c_str());
				ImGui::Text(std::format("Shadow array element height: {}",
					shadowMetadata.m_shadowArray->GetTextureParams().m_height).c_str());
				ImGui::Unindent();
			};


		auto ShowIndexMappings = [](LightMetadata const& lightMetadata, ShadowMetadata const& shadowMetadata)
			{
				const int numCols = 3;
				if (ImGui::BeginTable("Light/Shadow index mappings", numCols, k_tableFlags))
				{
					// Headers:				
					ImGui::TableSetupColumn("RenderDataID");
					ImGui::TableSetupColumn("LightData buffer index");
					ImGui::TableSetupColumn("Shadow array index");

					ImGui::TableHeadersRow();

					// Loop over light RenderDataIDs: All shadows have a light, but not all lights have a shadow
					for (auto const& lightEntry : lightMetadata.m_renderDataIDToBufferIdx)
					{
						ImGui::TableNextRow();
						ImGui::TableNextColumn();

						const gr::RenderDataID lightID = lightEntry.first;
						ImGui::Text(std::format("{}", lightID).c_str());

						ImGui::TableNextColumn();

						const uint32_t bufferIdx = lightEntry.second;
						ImGui::Text(std::format("{}", bufferIdx).c_str());

						ImGui::TableNextColumn();

						if (shadowMetadata.m_renderDataIDToTexArrayIdx.contains(lightID))
						{
							ImGui::Text(std::format("{}", shadowMetadata.m_renderDataIDToTexArrayIdx.at(lightID)).c_str());
						}
						else
						{
							ImGui::Text("-");
						}
					}

					ImGui::EndTable();
				}
			};


		if (ImGui::CollapsingHeader("Directional Lights", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShowLightMetadata(m_directionalLightMetadata);
			ImGui::NewLine();
			ShowShadowMetadata(m_directionalShadowMetadata);
			ImGui::NewLine();
			ShowIndexMappings(m_directionalLightMetadata, m_directionalShadowMetadata);
		}

		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Point Lights", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShowLightMetadata(m_pointLightMetadata);
			ImGui::NewLine();
			ShowShadowMetadata(m_pointShadowMetadata);
			ImGui::NewLine();
			ShowIndexMappings(m_pointLightMetadata, m_pointShadowMetadata);
		}

		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Spot Lights", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShowLightMetadata(m_spotLightMetadata);
			ImGui::NewLine();
			ShowShadowMetadata(m_spotShadowMetadata);
			ImGui::NewLine();
			ShowIndexMappings(m_spotLightMetadata, m_spotShadowMetadata);
		}
	}
}