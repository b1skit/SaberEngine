// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystemManager.h"
#include "GraphicsSystem.h"
#include "NamedObject.h"
#include "RenderPipeline.h"
#include "RenderPipelineDesc.h"


namespace re
{
	class RenderSystem : public virtual en::NamedObject
	{
	public:
		[[nodiscard]] static std::unique_ptr<RenderSystem> Create(std::string const& name);
		
		void Destroy();

		~RenderSystem() { Destroy(); };

		RenderSystem(RenderSystem&&) = default;
		RenderSystem& operator=(RenderSystem&&) = default;


	public:
		// Scriptable rendering pipeline:
		void BuildPipeline(RenderPipelineDesc::RenderSystemDescription const&);
		void ExecuteCreationPipeline();
		void ExecuteInitializationPipeline();
		void ExecuteUpdatePipeline();


	public:
		gr::GraphicsSystemManager& GetGraphicsSystemManager();

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
			std::unordered_map<std::string, void const*> m_resolvedDependencies;
			
			gr::GraphicsSystem const* m_gs; // Convenience for debugging/logging
			std::string const m_scriptFunctionName;
		};
		std::vector<UpdateStep> m_updatePipeline;


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


	inline re::RenderPipeline& RenderSystem::GetRenderPipeline()
	{
		return m_renderPipeline;
	}
}

