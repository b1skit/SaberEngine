// © 2023 Adam Badke. All rights reserved.
#include "Core/Assert.h"
#include "Buffer.h"
#include "GraphicsSystemManager.h"
#include "GraphicsSystem.h"
#include "Core/Util/ImGuiUtils.h"
#include "LightRenderData.h"
#include "RenderManager.h"
#include "RenderSystem.h"


namespace gr
{
	GraphicsSystemManager::GraphicsSystemManager(gr::RenderSystem* owningRS)
		: m_renderData(nullptr)
		, m_owningRenderSystem(owningRS)
		, m_activeCameraRenderDataID(gr::k_invalidRenderDataID)
		, m_activeCameraTransformDataID(gr::k_invalidTransformID)
		, m_activeAmbientLightRenderDataID(gr::k_invalidTransformID)
		, m_activeAmbientLightHasChanged(true)
	{
	}


	void GraphicsSystemManager::Destroy()
	{
		m_graphicsSystems.clear();
		m_renderData = nullptr;
	}


	void GraphicsSystemManager::Create()
	{
		re::RenderManager* renderManager = re::RenderManager::Get();

		m_renderData = &renderManager->GetRenderDataManager();

		CameraData defaultCameraParams{}; // Initialize with defaults, we'll update during PreRender()

		m_activeCameraParams = re::BufferInput(
			CameraData::s_shaderName,
			re::Buffer::Create(
				CameraData::s_shaderName,
				defaultCameraParams,
				re::Buffer::BufferParams{
					.m_stagingPool = re::Buffer::StagingPool::Permanent,
					.m_memPoolPreference = re::Buffer::DefaultHeap,
					.m_accessMask = re::Buffer::GPURead,
					.m_usageMask = re::Buffer::Constant,
				}));
	}


	void GraphicsSystemManager::PreRender()
	{
		if (m_activeCameraRenderDataID != gr::k_invalidRenderDataID &&
			m_activeCameraTransformDataID != gr::k_invalidTransformID)
		{
			gr::Camera::RenderData const& cameraData =
				m_renderData->GetObjectData<gr::Camera::RenderData>(m_activeCameraRenderDataID);

			m_activeCameraParams.GetBuffer()->Commit(cameraData.m_cameraParams);
		}

		UpdateActiveAmbientLight();
	}


	void GraphicsSystemManager::CreateAddGraphicsSystemByScriptName(char const* scriptName)
	{
		std::string lowercaseScriptName(util::ToLower(scriptName));

		SEAssert(!m_scriptNameToIndex.contains(lowercaseScriptName), "Graphics system has already been added");

		std::unique_ptr<gr::GraphicsSystem> newGS = gr::GraphicsSystem::CreateByName(lowercaseScriptName, this);
		SEAssert(newGS, "Failed to create a valid graphics system");

		const size_t insertIdx = m_graphicsSystems.size();
		m_graphicsSystems.emplace_back(std::move(newGS));
		m_scriptNameToIndex.emplace(std::move(lowercaseScriptName), insertIdx);
	}


	void GraphicsSystemManager::CreateAddGraphicsSystemByScriptName(std::string const& scriptName)
	{
		return CreateAddGraphicsSystemByScriptName(scriptName.c_str());
	}


	gr::GraphicsSystem* GraphicsSystemManager::GetGraphicsSystemByScriptName(char const* scriptName) const
	{
		std::string const& lowercaseScriptName(util::ToLower(scriptName));

		if (m_scriptNameToIndex.contains(lowercaseScriptName))
		{
			return m_graphicsSystems[m_scriptNameToIndex.at(lowercaseScriptName)].get();
		}
		return nullptr;
	}


	gr::GraphicsSystem* GraphicsSystemManager::GetGraphicsSystemByScriptName(std::string const& scriptName) const
	{
		return GetGraphicsSystemByScriptName(scriptName.c_str());
	}


	void GraphicsSystemManager::EndOfFrame()
	{
		for (auto& gs : m_graphicsSystems)
		{
			gs->EndOfFrame();
		}
	}


	void GraphicsSystemManager::UpdateActiveAmbientLight()
	{
		// Reset our active ambient changed flag for the new frame:
		m_activeAmbientLightHasChanged = false;

		// Update the active ambient light:
		// First, check if our currently active light has been deleted:
		std::vector<gr::RenderDataID> const* deletedAmbientLights =
			m_renderData->GetIDsWithDeletedData<gr::Light::RenderDataAmbientIBL>();
		if (deletedAmbientLights)
		{
			for (gr::RenderDataID ambientID : *deletedAmbientLights)
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
			m_renderData->IsDirty<gr::Light::RenderDataAmbientIBL>(m_activeAmbientLightRenderDataID))
		{
			gr::Light::RenderDataAmbientIBL const& activeAmbientData =
				m_renderData->GetObjectData<gr::Light::RenderDataAmbientIBL>(m_activeAmbientLightRenderDataID);

			if (!activeAmbientData.m_isActive)
			{
				m_activeAmbientLightRenderDataID = gr::k_invalidRenderDataID;
				m_activeAmbientLightHasChanged = true;
			}
		}

		// If we don't have an active light, see if any exist in the render data:
		if (m_activeAmbientLightRenderDataID == gr::k_invalidRenderDataID &&
			m_renderData->HasObjectData<gr::Light::RenderDataAmbientIBL>())
		{
			auto ambientItr = m_renderData->LinearBegin<gr::Light::RenderDataAmbientIBL>();
			auto const& ambientItrEnd = m_renderData->LinearEnd<gr::Light::RenderDataAmbientIBL>();
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
		}
	}


	re::BufferInput const& GraphicsSystemManager::GetActiveCameraParams() const
	{
		SEAssert(m_activeCameraParams.IsValid(), "Camera buffer has not been created");
		return m_activeCameraParams;
	}


	void GraphicsSystemManager::SetActiveCamera(gr::RenderDataID cameraRenderDataID, gr::TransformID cameraTransformID)
	{
		SEAssert((cameraRenderDataID != gr::k_invalidRenderDataID) == (cameraTransformID != gr::k_invalidTransformID),
			"Invalid ID: Must both be valid or invalid");

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
		for (std::unique_ptr<gr::GraphicsSystem> const& gs : m_graphicsSystems)
		{
			if (ImGui::CollapsingHeader(std::format("{}##{}", gs->GetName(), gs->GetUniqueID()).c_str()))
			{
				ImGui::Indent();
				gs->ShowImGuiWindow();
				ImGui::Unindent();
			}
		}
	}
}