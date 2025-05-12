// © 2024 Adam Badke. All rights reserved.
#include "GraphicsSystem_LightManager.h"
#include "GraphicsSystemManager.h"
#include "LightParamsHelpers.h"
#include "RenderDataManager.h"

#include "Core/Config.h"

#include "Shaders/Common/LightParams.h"


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
		// Shadow array textures:
		RegisterTextureOutput(k_directionalShadowArrayTexOutput, &m_directionalShadowMetadata.m_shadowArray);
		RegisterTextureOutput(k_pointShadowArrayTexOutput, &m_pointShadowMetadata.m_shadowArray);
		RegisterTextureOutput(k_spotShadowArrayTexOutput, &m_spotShadowMetadata.m_shadowArray);
		
		RegisterDataOutput(k_lightIDToShadowRecordOutput, &m_lightIDToShadowRecords);

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
		uint32_t shadowTexArrayIdx = INVALID_SHADOW_IDX;
		if (shadowMetadata.m_renderDataIDToTexArrayIdx.contains(lightID))
		{
			shadowTexArrayIdx = shadowMetadata.m_renderDataIDToTexArrayIdx.at(lightID);
		}
		return shadowTexArrayIdx;
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
		std::vector<gr::RenderDataID> const* deletedShadows = renderData.GetIDsWithDeletedData<gr::ShadowMap::RenderData>();
		if (deletedShadows && !deletedShadows->empty())
		{
			for (gr::RenderDataID deletedID : *deletedShadows)
			{
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

				// Update the shadow record output:
				SEAssert(m_lightIDToShadowRecords.contains(deletedID), "Failed to find the light ID");
				m_lightIDToShadowRecords.erase(deletedID);
			}
		}
	}


	void LightManagerGraphicsSystem::RegisterNewLights(gr::RenderDataManager const& renderData)
	{
		std::vector<gr::RenderDataID> const* newShadows = renderData.GetIDsWithNewData<gr::ShadowMap::RenderData>();
		if (newShadows && !newShadows->empty())
		{
			for (auto const& shadowItr : gr::IDAdapter(renderData, *newShadows))
			{
				const gr::RenderDataID shadowID = shadowItr->GetRenderDataID();

				gr::ShadowMap::RenderData const& shadowMapRenderData = shadowItr->Get<gr::ShadowMap::RenderData>();

				auto AddShadowToMetadata = [&shadowID, this](ShadowMetadata& shadowMetadata)
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

						// Update the shadow record output:
						SEAssert(m_lightIDToShadowRecords.contains(shadowID) == false, "RenderDataID already registered");
						m_lightIDToShadowRecords.emplace(
							shadowID,
							gr::ShadowRecord{
								.m_shadowTex = &shadowMetadata.m_shadowArray,
								.m_shadowTexArrayIdx = newShadowIndex,
							});
					};

				switch (shadowMapRenderData.m_lightType)
				{
				case gr::Light::Type::Directional: AddShadowToMetadata(m_directionalShadowMetadata); break;
				case gr::Light::Type::Point: AddShadowToMetadata(m_pointShadowMetadata); break;
				case gr::Light::Type::Spot: AddShadowToMetadata(m_spotShadowMetadata); break;
				case gr::Light::Type::AmbientIBL:
				default: SEAssertF("Invalid light type");
				}
			}
		}
	}


	void LightManagerGraphicsSystem::UpdateLightBufferData(gr::RenderDataManager const& renderData)
	{
		auto UpdateShadowTexture = [this](
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
					shadowArrayParams.m_optimizedClear.m_depthStencil.m_depth = 1.f;

					// Cache the current shadow texture address before we replace it:
					core::InvPtr<re::Texture> const* prevShadowTex = &shadowMetadata.m_shadowArray;

					shadowMetadata.m_shadowArray = re::Texture::Create(shadowTexName, shadowArrayParams);

					// Update the existing shadow record outputs with the new texture:
					uint32_t newArrayIdx = 0;
					for (auto& entry : m_lightIDToShadowRecords)
					{
						if (entry.second.m_shadowTex == prevShadowTex)
						{
							SEAssert(newArrayIdx < shadowArrayParams.m_arraySize,
								"New shadow texture array index is out of bounds");
							entry.second.m_shadowTex = &shadowMetadata.m_shadowArray;
							entry.second.m_shadowTexArrayIdx = newArrayIdx++;
						}
					}					
				}
			};
		UpdateShadowTexture(gr::Light::Directional, m_directionalShadowMetadata, "Directional shadows");
		UpdateShadowTexture(gr::Light::Point, m_pointShadowMetadata, "Point shadows");
		UpdateShadowTexture(gr::Light::Spot, m_spotShadowMetadata, "Spot shadows");
	}


	void LightManagerGraphicsSystem::ShowImGuiWindow()
	{
		constexpr ImGuiTableFlags k_tableFlags =
			ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;

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


		if (ImGui::CollapsingHeader("Directional Lights", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShowShadowMetadata(m_directionalShadowMetadata);
		}

		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Point Lights", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShowShadowMetadata(m_pointShadowMetadata);
		}

		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Spot Lights", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShowShadowMetadata(m_spotShadowMetadata);
		}
	}
}