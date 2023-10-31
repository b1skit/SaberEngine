// © 2023 Adam Badke. All rights reserved.
#include "RenderSystem.h"


namespace re
{
	std::unique_ptr<RenderSystem> RenderSystem::Create(std::string const& name)
	{
		std::unique_ptr<RenderSystem> newRenderSystem = nullptr;

		newRenderSystem.reset(new RenderSystem(name));

		return std::move(newRenderSystem);
	}


	RenderSystem::RenderSystem(std::string const& name)
		: NamedObject(name)
		, m_renderPipeline(name + " render pipeline")
		, m_createPipeline(nullptr)
		, m_updatePipeline(nullptr)
	{
	}


	void RenderSystem::Destroy()
	{
		m_graphicsSystems.clear();
		m_renderPipeline.Destroy();
		m_createPipeline = nullptr;
		m_updatePipeline = nullptr;
	}


	void RenderSystem::ExecuteCreatePipeline()
	{
		m_createPipeline(this);
	}


	void RenderSystem::ExecuteUpdatePipeline()
	{
		m_updatePipeline(this);
	}


	void RenderSystem::ShowImGuiWindow()
	{
		for (std::shared_ptr<gr::GraphicsSystem> const& gs : m_graphicsSystems)
		{
			if (ImGui::CollapsingHeader(std::format("{}##{}", gs->GetName(), gs->GetUniqueID()).c_str()))
			{
				gs->ShowImGuiWindow();
			}
		}
	}
}