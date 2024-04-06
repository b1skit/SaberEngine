// © 2023 Adam Badke. All rights reserved.
#include "GraphicsSystem_Culling.h"
#include "GraphicsSystemManager.h"
#include "GraphicsSystem.h"
#include "ImGuiUtils.h"
#include "LightRenderData.h"
#include "Buffer.h"
#include "RenderSystem.h"


namespace gr
{
	GraphicsSystemManager::GraphicsSystemManager(re::RenderSystem* owningRS)
		: m_owningRenderSystem(owningRS)
		, m_activeCameraRenderDataID(gr::k_invalidRenderDataID)
		, m_activeCameraTransformDataID(gr::k_invalidTransformID)
		, m_activeCameraParams(nullptr)
		, m_activeAmbientLightRenderDataID(gr::k_invalidTransformID)
		, m_activeAmbientLightHasChanged(true)
	{
	}


	void GraphicsSystemManager::Destroy()
	{
		m_graphicsSystems.clear();
		m_renderData.Destroy();
	}


	void GraphicsSystemManager::Create()
	{
		CameraData defaultCameraParams{}; // Initialize with defaults, we'll update during PreRender()

		m_activeCameraParams = re::Buffer::Create(
			CameraData::s_shaderName,
			defaultCameraParams,
			re::Buffer::Type::Mutable);
	}


	void GraphicsSystemManager::PreRender()
	{
		m_batchManager.UpdateBatchCache(m_renderData);

		SEAssert(m_activeCameraRenderDataID != gr::k_invalidRenderDataID && 
			m_activeCameraTransformDataID != gr::k_invalidTransformID,
			"No active camera has been set");

		gr::Camera::RenderData const& cameraData =
			m_renderData.GetObjectData<gr::Camera::RenderData>(m_activeCameraRenderDataID);

		m_activeCameraParams->Commit(cameraData.m_cameraParams);

		UpdateActiveAmbientLight();
	}


	void GraphicsSystemManager::CreateAddGraphicsSystemByScriptName(char const* scriptName)
	{
		std::string lowercaseScriptName(util::ToLower(scriptName));

		SEAssert(!m_scriptNameToIndex.contains(lowercaseScriptName), "Graphics system has already been added");

		std::shared_ptr<gr::GraphicsSystem> newGS = gr::GraphicsSystem::CreateByName(lowercaseScriptName, this);
		SEAssert(newGS, "Failed to create a valid graphics system");

		const size_t insertIdx = m_graphicsSystems.size();
		m_graphicsSystems.emplace_back(newGS);
		m_scriptNameToIndex.emplace(std::move(lowercaseScriptName), insertIdx);
	}


	void GraphicsSystemManager::CreateAddGraphicsSystemByScriptName(std::string const& scriptName)
	{
		return CreateAddGraphicsSystemByScriptName(scriptName.c_str());
	}


	gr::GraphicsSystem* GraphicsSystemManager::GetGraphicsSystemByScriptName(char const* scriptName)
	{
		std::string const& lowercaseScriptName(util::ToLower(scriptName));
		SEAssert(m_scriptNameToIndex.contains(lowercaseScriptName), "No GraphicsSystem with that script name exists");

		return m_graphicsSystems[m_scriptNameToIndex.at(lowercaseScriptName)].get();
	}


	gr::GraphicsSystem* GraphicsSystemManager::GetGraphicsSystemByScriptName(std::string const& scriptName)
	{
		return GetGraphicsSystemByScriptName(scriptName.c_str());
	}


	void GraphicsSystemManager::UpdateActiveAmbientLight()
	{
		// Reset our active ambient changed flag for the new frame:
		m_activeAmbientLightHasChanged = false;

		// Update the active ambient light:
		// First, check if our currently active light has been deleted:
		if (m_renderData.HasIDsWithDeletedData<gr::Light::RenderDataAmbientIBL>())
		{
			std::vector<gr::RenderDataID> const& deletedAmbientLights =
				m_renderData.GetIDsWithDeletedData<gr::Light::RenderDataAmbientIBL>();

			for (gr::RenderDataID ambientID : deletedAmbientLights)
			{
				if (ambientID == m_activeAmbientLightRenderDataID)
				{
					m_activeAmbientLightRenderDataID = gr::k_invalidRenderDataID;
					m_activeAmbientLightHasChanged = true;
					break;
				}
			}
		}

		// If we have an active ambient light, check that it is still actually active:
		if (m_activeAmbientLightRenderDataID != gr::k_invalidRenderDataID &&
			m_renderData.IsDirty<gr::Light::RenderDataAmbientIBL>(m_activeAmbientLightRenderDataID))
		{
			gr::Light::RenderDataAmbientIBL const& activeAmbientData =
				m_renderData.GetObjectData<gr::Light::RenderDataAmbientIBL>(m_activeAmbientLightRenderDataID);

			if (!activeAmbientData.m_isActive)
			{
				m_activeAmbientLightRenderDataID = gr::k_invalidRenderDataID;
				m_activeAmbientLightHasChanged = true;
			}
		}

		// If we don't have an active light, see if any exist in the render data:
		if (m_activeAmbientLightRenderDataID == gr::k_invalidRenderDataID &&
			m_renderData.HasObjectData<gr::Light::RenderDataAmbientIBL>())
		{
			auto ambientItr = m_renderData.Begin<gr::Light::RenderDataAmbientIBL>();
			auto const& ambientItrEnd = m_renderData.End<gr::Light::RenderDataAmbientIBL>();
			while (ambientItr != ambientItrEnd)
			{
				if (ambientItr->m_isActive)
				{
					m_activeAmbientLightRenderDataID = ambientItr->m_renderDataID;
					m_activeAmbientLightHasChanged = true;
					break;
				}
				++ambientItr;
			}
			SEAssert(m_activeAmbientLightRenderDataID != gr::k_invalidRenderDataID,
				"Failed to find an active ambient light. This should not be possible");
		}
	}


	std::vector<re::Batch> GraphicsSystemManager::GetVisibleBatches(
		gr::Camera::View const& cameraView,
		uint8_t bufferTypeMask/*= (gr::BatchManager::InstanceType::Transform | gr::BatchManager::InstanceType::Material)*/) const
	{
		gr::CullingGraphicsSystem const* cullingGS = GetGraphicsSystem<gr::CullingGraphicsSystem>();

		return m_batchManager.BuildSceneBatches(
			m_renderData,
			cullingGS->GetVisibleRenderDataIDs(cameraView),
			bufferTypeMask);
	}


	std::vector<re::Batch> GraphicsSystemManager::GetVisibleBatches(
		std::vector<gr::Camera::View> const& views,
		uint8_t bufferTypeMask/*= (gr::BatchManager::InstanceType::Transform | gr::BatchManager::InstanceType::Material)*/) const
	{
		gr::CullingGraphicsSystem const* cullingGS = GetGraphicsSystem<gr::CullingGraphicsSystem>();

		std::vector<gr::RenderDataID> uniqueRenderDataIDs;
		uniqueRenderDataIDs.reserve(m_renderData.GetNumElementsOfType<gr::MeshPrimitive::RenderData>());

		// Combine the RenderDataIDs visible in each view into a unique set
		std::unordered_set<gr::RenderDataID> seenIDs;
		seenIDs.reserve(m_renderData.GetNumElementsOfType<gr::MeshPrimitive::RenderData>());
		for (gr::Camera::View const& view : views)
		{
			std::vector<gr::RenderDataID> const& visibleIDs = cullingGS->GetVisibleRenderDataIDs(view);
			for (gr::RenderDataID id : visibleIDs)
			{
				if (!seenIDs.contains(id))
				{
					seenIDs.emplace(id);
					uniqueRenderDataIDs.emplace_back(id);
				}
			}
		}

		// Build batches from the final set of ids:
		return m_batchManager.BuildSceneBatches(
			m_renderData,
			uniqueRenderDataIDs,
			bufferTypeMask);
	}


	gr::Camera::RenderData const& GraphicsSystemManager::GetActiveCameraRenderData() const
	{
		SEAssert(m_activeCameraRenderDataID != gr::k_invalidRenderDataID, "No active camera has been set");
		return m_renderData.GetObjectData< gr::Camera::RenderData>(m_activeCameraRenderDataID);
	}


	gr::Transform::RenderData const& GraphicsSystemManager::GetActiveCameraTransformData() const
	{
		SEAssert(m_activeCameraTransformDataID != gr::k_invalidTransformID, "No active camera has been set");
		return m_renderData.GetTransformDataFromTransformID(m_activeCameraTransformDataID);
	}


	std::shared_ptr<re::Buffer> GraphicsSystemManager::GetActiveCameraParams() const
	{
		SEAssert(m_activeCameraParams != nullptr, "Camera buffer has not been created");
		return m_activeCameraParams;
	}


	void GraphicsSystemManager::SetActiveCamera(gr::RenderDataID cameraRenderDataID, gr::TransformID cameraTransformID)
	{
		SEAssert(cameraRenderDataID != gr::k_invalidRenderDataID && cameraTransformID != gr::k_invalidTransformID,
			"Invalid ID");

		m_activeCameraRenderDataID = cameraRenderDataID;
		m_activeCameraTransformDataID = cameraTransformID;
	}


	bool GraphicsSystemManager::ActiveAmbientLightHasChanged() const
	{
		return m_activeAmbientLightHasChanged;
	}


	bool GraphicsSystemManager::HasActiveAmbientLight() const
	{
		return m_activeAmbientLightRenderDataID != gr::k_invalidRenderDataID;
	}


	gr::RenderDataID GraphicsSystemManager::GetActiveAmbientLightID() const
	{
		return m_activeAmbientLightRenderDataID;
	}


	void GraphicsSystemManager::ShowImGuiWindow()
	{
		for (std::shared_ptr<gr::GraphicsSystem> const& gs : m_graphicsSystems)
		{
			if (ImGui::CollapsingHeader(std::format("{}##{}", gs->GetName(), gs->GetUniqueID()).c_str()))
			{
				ImGui::Indent();
				gs->ShowImGuiWindow();
				ImGui::Unindent();
			}
		}
	}


	void GraphicsSystemManager::ShowImGuiRenderDataDebugWindow() const
	{
		m_renderData.ShowImGuiWindow();
	}
}