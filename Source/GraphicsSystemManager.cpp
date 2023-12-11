// © 2023 Adam Badke. All rights reserved.
#include "GraphicsSystemManager.h"
#include "GraphicsSystem.h"
#include "RenderCommand.h"
#include "RenderSystem.h"


namespace gr
{
	GraphicsSystemManager::GraphicsSystemManager(re::RenderSystem* owningRS)
		: m_owningRenderSystem(owningRS)
	{
	}


	void GraphicsSystemManager::Destroy()
	{
		m_graphicsSystems.clear();
		m_renderData.Destroy();
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
}