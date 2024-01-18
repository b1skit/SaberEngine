// © 2023 Adam Badke. All rights reserved.
#include "GraphicsSystemManager.h"
#include "GraphicsSystem.h"
#include "ImGuiUtils.h"
#include "RenderSystem.h"
#include "ParameterBlock.h"


namespace gr
{
	GraphicsSystemManager::GraphicsSystemManager(re::RenderSystem* owningRS)
		: m_owningRenderSystem(owningRS)
		, m_activeCameraRenderDataID(gr::k_invalidRenderDataID)
		, m_activeCameraTransformDataID(gr::k_invalidTransformID)
		, m_activeCameraParams(nullptr)
	{
	}


	void GraphicsSystemManager::Destroy()
	{
		m_graphicsSystems.clear();
		m_renderData.Destroy();
	}


	void GraphicsSystemManager::Create()
	{
		gr::Camera::CameraParams defaultCameraParams{}; // Initialize with defaults, we'll update during PreRender()

		m_activeCameraParams = re::ParameterBlock::Create(
			gr::Camera::CameraParams::s_shaderName,
			defaultCameraParams,
			re::ParameterBlock::PBType::Mutable);
	}


	void GraphicsSystemManager::PreRender()
	{
		SEAssert(m_activeCameraRenderDataID != gr::k_invalidRenderDataID && 
			m_activeCameraTransformDataID != gr::k_invalidTransformID,
			"No active camera has been set");

		gr::Camera::RenderData const& cameraData =
			m_renderData.GetObjectData<gr::Camera::RenderData>(m_activeCameraRenderDataID);

		m_activeCameraParams->Commit(cameraData.m_cameraParams);
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


	std::shared_ptr<re::ParameterBlock> GraphicsSystemManager::GetActiveCameraParams() const
	{
		SEAssert(m_activeCameraParams != nullptr, "Camera parameter block has not been created");
		return m_activeCameraParams;
	}


	void GraphicsSystemManager::SetActiveCamera(gr::RenderDataID cameraRenderDataID, gr::TransformID cameraTransformID)
	{
		SEAssert(cameraRenderDataID != gr::k_invalidRenderDataID && cameraTransformID != gr::k_invalidTransformID,
			"Invalid ID");

		m_activeCameraRenderDataID = cameraRenderDataID;
		m_activeCameraTransformDataID = cameraTransformID;
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