// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystemManager.h"
#include "GraphicsSystem.h"
#include "RenderPipeline.h"

#include "Core/Interfaces/INamedObject.h"


namespace re
{
	struct RenderSystemDescription;


	class RenderSystem : public virtual core::INamedObject
	{
	public:
		[[nodiscard]] static std::unique_ptr<RenderSystem> Create(
			std::string const& name, std::string const& pipelineFileName);
		
		void Destroy();

		~RenderSystem() { Destroy(); };

		RenderSystem(RenderSystem&&) = default;
		RenderSystem& operator=(RenderSystem&&) = default;


	public:
		// Scriptable rendering pipeline:
		void BuildPipeline(RenderSystemDescription const&); // Creates graphics systems + init/update pipelines
		void ExecuteInitializationPipeline();
		void ExecuteUpdatePipeline();


	public:
		void PostUpdatePreRender(); // Called after ExecuteUpdatePipeline()
		void EndOfFrame();


	public:
		gr::GraphicsSystemManager& GetGraphicsSystemManager();
		gr::GraphicsSystemManager const& GetGraphicsSystemManager() const;

		re::RenderPipeline& GetRenderPipeline();

		void ShowImGuiWindow();


	private:
		gr::GraphicsSystemManager m_graphicsSystemManager;

		re::RenderPipeline m_renderPipeline;
		std::function<void(re::RenderSystem*)> m_creationPipeline;
		std::function<void(re::RenderSystem*)> m_initPipeline;

	private:
		struct UpdateStep
		{
			gr::GraphicsSystem::RuntimeBindings::PreRenderFn m_preRenderFunc;
			std::unordered_map<util::HashKey const, void const*> m_resolvedDependencies;
			
			// Convenience for debugging/logging:
			gr::GraphicsSystem const* m_gs; 
			std::string const m_scriptFunctionName;
		};

		std::vector<std::vector<UpdateStep>> m_updatePipeline;


	private: // Use the Create() factory
		RenderSystem(std::string const& name);
		RenderSystem() = delete; 


	private: // No copying allowed:
		RenderSystem(RenderSystem const&) = delete;
		RenderSystem& operator=(RenderSystem const&) = delete;
	};


	inline gr::GraphicsSystemManager& RenderSystem::GetGraphicsSystemManager()
	{
		return m_graphicsSystemManager;
	}


	inline gr::GraphicsSystemManager const& RenderSystem::GetGraphicsSystemManager() const
	{
		return m_graphicsSystemManager;
	}


	inline re::RenderPipeline& RenderSystem::GetRenderPipeline()
	{
		return m_renderPipeline;
	}
}

