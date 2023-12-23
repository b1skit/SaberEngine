// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystemManager.h"
#include "NamedObject.h"
#include "RenderPipeline.h"


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

		void SetInitializePipeline(std::function<void(re::RenderSystem*)>);
		void ExecuteInitializePipeline();

		void SetCreatePipeline(std::function<void(re::RenderSystem*)>);
		void ExecuteCreatePipeline();

		void SetUpdatePipeline(std::function<void(re::RenderSystem*)>);
		void ExecuteUpdatePipeline();

		gr::GraphicsSystemManager& GetGraphicsSystemManager();

		re::RenderPipeline& GetRenderPipeline();

		void ShowImGuiWindow();


	private:
		gr::GraphicsSystemManager m_graphicsSystemManager;

		re::RenderPipeline m_renderPipeline;
		std::function<void(re::RenderSystem*)> m_initializePipeline;
		std::function<void(re::RenderSystem*)> m_createPipeline;
		std::function<void(re::RenderSystem*)> m_updatePipeline;


	private: // Use the Create() factory
		RenderSystem(std::string const& name);
		RenderSystem() = delete; 


	private: // No copying allowed:
		RenderSystem(RenderSystem const&) = delete;
		RenderSystem& operator=(RenderSystem const&) = delete;
	};


	inline void RenderSystem::SetInitializePipeline(std::function<void(re::RenderSystem*)> initializePipeline)
	{
		m_initializePipeline = initializePipeline;
	}


	inline void RenderSystem::SetCreatePipeline(std::function<void(re::RenderSystem*)> createPipeline)
	{
		m_createPipeline = createPipeline;
	}


	inline void RenderSystem::SetUpdatePipeline(std::function<void(re::RenderSystem*)> updatePipeline)
	{
		m_updatePipeline = updatePipeline;
	}


	inline gr::GraphicsSystemManager& RenderSystem::GetGraphicsSystemManager()
	{
		return m_graphicsSystemManager;
	}


	inline re::RenderPipeline& RenderSystem::GetRenderPipeline()
	{
		return m_renderPipeline;
	}
}

