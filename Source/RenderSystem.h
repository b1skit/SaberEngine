// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"
#include "NamedObject.h"
#include "RenderPipeline.h"


namespace re
{
	class RenderSystem : public virtual en::NamedObject
	{
	public:
		struct PipelineLambdas
		{
			std::function<void()> m_createPipeline;
			std::function<void()> m_updatePipeline;
		};

	public:
		static std::unique_ptr<RenderSystem> Create(std::string const& name);
		
		void Destroy();

		~RenderSystem() { Destroy(); };

		RenderSystem(RenderSystem&&) = default;
		RenderSystem& operator=(RenderSystem&&) = default;

		void SetCreatePipeline(std::function<void(re::RenderSystem*)>);
		void ExecuteCreatePipeline();

		void SetUpdatePipeline(std::function<void(re::RenderSystem*)>);
		void ExecuteUpdatePipeline();

		std::vector<std::shared_ptr<gr::GraphicsSystem>>& GetGraphicsSystems();
		
		template <typename T>
		T* GetGraphicsSystem();

		re::RenderPipeline& GetRenderPipeline();

		void ShowImGuiWindow();


	private:
		std::vector<std::shared_ptr<gr::GraphicsSystem>> m_graphicsSystems;
		re::RenderPipeline m_renderPipeline;

		std::function<void(re::RenderSystem*)> m_createPipeline;
		std::function<void(re::RenderSystem*)> m_updatePipeline;


	private: // Use the Create() factory
		RenderSystem(std::string const& name);
		RenderSystem() = delete; 


	private: // No copying allowed:
		RenderSystem(RenderSystem const&) = delete;
		RenderSystem& operator=(RenderSystem const&) = delete;
	};


	inline void RenderSystem::SetCreatePipeline(std::function<void(re::RenderSystem*)> createPipeline)
	{
		m_createPipeline = createPipeline;
	}


	inline void RenderSystem::SetUpdatePipeline(std::function<void(re::RenderSystem*)> updatePipeline)
	{
		m_updatePipeline = updatePipeline;
	}


	inline std::vector<std::shared_ptr<gr::GraphicsSystem>>& RenderSystem::GetGraphicsSystems()
	{
		return m_graphicsSystems;
	}


	template <typename T>
	T* RenderSystem::GetGraphicsSystem()
	{
		// TODO: A linear search isn't optimal here, but there aren't many graphics systems in practice so ok for now
		for (size_t i = 0; i < m_graphicsSystems.size(); i++)
		{
			T* result = dynamic_cast<T*>(m_graphicsSystems[i].get());
			if (result != nullptr)
			{
				return result;
			}
		}

		SEAssertF("Graphics system not found");
		return nullptr;
	}


	inline re::RenderPipeline& RenderSystem::GetRenderPipeline()
	{
		return m_renderPipeline;
	}
}

