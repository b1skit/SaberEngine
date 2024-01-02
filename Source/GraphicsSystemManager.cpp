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
		, m_activeCameraRenderDataID(gr::k_invalidRenderObjectID)
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
		SEAssert("No active camera has been set",
			m_activeCameraRenderDataID != gr::k_invalidRenderObjectID && 
			m_activeCameraTransformDataID != gr::k_invalidTransformID);

		gr::Camera::RenderData const& cameraData =
			m_renderData.GetObjectData<gr::Camera::RenderData>(m_activeCameraRenderDataID);

		m_activeCameraParams->Commit(cameraData.m_cameraParams);
	}


	gr::Camera::RenderData const& GraphicsSystemManager::GetActiveCameraRenderData() const
	{
		SEAssert("No active camera has been set", m_activeCameraRenderDataID != gr::k_invalidRenderObjectID);
		return m_renderData.GetObjectData< gr::Camera::RenderData>(m_activeCameraRenderDataID);
	}


	gr::Transform::RenderData const& GraphicsSystemManager::GetActiveCameraTransformData() const
	{
		SEAssert("No active camera has been set", m_activeCameraTransformDataID != gr::k_invalidTransformID);
		return m_renderData.GetTransformData(m_activeCameraTransformDataID);
	}


	std::shared_ptr<re::ParameterBlock> GraphicsSystemManager::GetActiveCameraParams() const
	{
		SEAssert("Camera parameter block has not been created", m_activeCameraParams != nullptr);
		return m_activeCameraParams;
	}


	void GraphicsSystemManager::SetActiveCamera(gr::RenderDataID cameraRenderDataID, gr::TransformID cameraTransformID)
	{
		SEAssert("Invalid ID", 
			cameraRenderDataID != gr::k_invalidRenderObjectID && cameraTransformID != gr::k_invalidTransformID);

		m_activeCameraRenderDataID = cameraRenderDataID;
		m_activeCameraTransformDataID = cameraTransformID;
	}


	void GraphicsSystemManager::ShowImGuiWindow()
	{
		if (ImGui::CollapsingHeader(std::format("Render data##", util::PtrToID(this)).c_str()))
		{
			ImGui::Indent();
			m_renderData.ShowImGuiWindow();
			ImGui::Unindent();
		}

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
}