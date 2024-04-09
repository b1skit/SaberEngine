// © 2023 Adam Badke. All rights reserved.
#include "ImGuiUtils.h"
#include "GraphicsSystem.h"
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
		, m_initPipeline(nullptr)
	{
	}


	void RenderSystem::Destroy()
	{
		m_graphicsSystemManager.Destroy();
		m_renderPipeline.Destroy();
		m_initPipeline = nullptr;
		m_updatePipeline.clear();
	}


	void RenderSystem::BuildPipeline(RenderPipelineDesc::RenderSystemDescription const& renderSysDesc)
	{
		// Create the GS creation pipeline:
		m_creationPipeline = [this, renderSysDesc](re::RenderSystem* renderSystem)
		{
			gr::GraphicsSystemManager& gsm = renderSystem->GetGraphicsSystemManager();

			for (std::string const& gsName : renderSysDesc.m_graphicsSystemNames)
			{
				gsm.CreateAddGraphicsSystemByScriptName(gsName);
			}

			// The update pipeline caches member function pointers; We can populate it now that our GS objects exist
			for (auto const& entry : renderSysDesc.m_updateSteps)
			{
				gr::GraphicsSystem* currentGS = gsm.GetGraphicsSystemByScriptName(entry.first);

				std::string const& lowercaseScriptFnName(util::ToLower(entry.second));

				m_updatePipeline.emplace_back(
					currentGS->GetRuntimeBindings().m_preRenderFunctions.at(lowercaseScriptFnName));
			}
		};

		m_initPipeline = [renderSysDesc](re::RenderSystem* renderSystem)
		{
			gr::GraphicsSystemManager& gsm = renderSystem->GetGraphicsSystemManager();
			gsm.Create();

			for (auto const& entry : renderSysDesc.m_initSteps)
			{
				gr::GraphicsSystem* currentGS = gsm.GetGraphicsSystemByScriptName(entry.first);

				std::string const& lowercaseScriptFnName(util::ToLower(entry.second));
				std::string const& stagePipelineName = std::format("{} stages", currentGS->GetName());

				currentGS->GetRuntimeBindings().m_initPipelineFunctions.at(lowercaseScriptFnName)(
					renderSystem->GetRenderPipeline().AddNewStagePipeline(stagePipelineName));
			}
		};

		// We can only build the update pipeline once the GS's have been created (as we need to access their runtime
		// bindings)
		m_updatePipeline.reserve(renderSysDesc.m_updateSteps.size());
	}


	void RenderSystem::ExecuteInitializePipeline()
	{
		m_creationPipeline(this);
	}


	void RenderSystem::ExecuteCreatePipeline()
	{
		m_initPipeline(this);
	}


	void RenderSystem::ExecuteUpdatePipeline()
	{
		SEBeginCPUEvent(std::format("{} ExecuteUpdatePipeline", GetName()).c_str());

		m_graphicsSystemManager.PreRender();

		for (auto executeStep : m_updatePipeline)
		{
			executeStep();
		}

		SEEndCPUEvent();
	}


	void RenderSystem::ShowImGuiWindow()
	{
		if (ImGui::CollapsingHeader(std::format("Graphics System Manager##", util::PtrToID(this)).c_str(), 
			ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent();
			m_graphicsSystemManager.ShowImGuiWindow();
			ImGui::Unindent();
		}
	}
}