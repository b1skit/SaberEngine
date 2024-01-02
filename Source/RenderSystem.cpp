// � 2023 Adam Badke. All rights reserved.
#include "ImGuiUtils.h"
#include "ProfilingMarkers.h"
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
		, m_graphicsSystemManager(this)
		, m_renderPipeline(name + " render pipeline")
		, m_createPipeline(nullptr)
		, m_updatePipeline(nullptr)
	{
	}


	void RenderSystem::Destroy()
	{
		m_graphicsSystemManager.Destroy();
		m_renderPipeline.Destroy();
		m_createPipeline = nullptr;
		m_updatePipeline = nullptr;
	}


	void RenderSystem::ExecuteInitializePipeline()
	{
		m_initializePipeline(this);
	}


	void RenderSystem::ExecuteCreatePipeline()
	{
		m_createPipeline(this);
	}


	void RenderSystem::ExecuteUpdatePipeline()
	{
		SEBeginCPUEvent(std::format("{} ExecuteUpdatePipeline", GetName()).c_str());

		m_updatePipeline(this);

		SEEndCPUEvent();
	}


	void RenderSystem::ShowImGuiWindow()
	{
		if (ImGui::CollapsingHeader(std::format("Graphics System Manager##", util::PtrToID(this)).c_str()))
		{
			ImGui::Indent();
			m_graphicsSystemManager.ShowImGuiWindow();
			ImGui::Unindent();
		}
	}
}