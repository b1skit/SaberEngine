// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Core/Assert.h"
#include "Core/CommandQueue.h"


namespace re
{
	class Context;
}
namespace gr
{
	class RenderManager;
	class RenderDataManager;
	class RenderSystem;

	class RenderCommand
	{
	public:
		template<typename T, typename... Args>
		requires std::derived_from<T, RenderCommand>
		static void Enqueue(Args&&... args);


	protected:
		// Guaranteed to be thread safe, as these are executed by the command queue
		gr::RenderDataManager const& GetRenderData();
		gr::RenderDataManager& GetRenderDataManagerForModification();

		std::vector<std::unique_ptr<gr::RenderSystem>> const& GetRenderSystems() const;
		std::vector<std::unique_ptr<gr::RenderSystem>>& GetRenderSystemsForModification() const;

		re::Context* GetContextForModification() const;


	private:
		friend class RenderManager;
		static core::CommandManager* s_renderCommandManager;
		static gr::RenderDataManager* s_renderDataManager;
		static std::vector<std::unique_ptr<gr::RenderSystem>>* s_renderSystems;
		static re::Context* s_context;
	};


	template<typename T, typename... Args>
	requires std::derived_from<T, RenderCommand>
	inline void RenderCommand::Enqueue(Args&&... args)
	{
		SEAssert(s_renderCommandManager, "Dependency is null");
		s_renderCommandManager->Enqueue<T>(std::forward<Args>(args)...);
	}


	inline gr::RenderDataManager const& RenderCommand::GetRenderData()
	{
		SEAssert(s_renderDataManager, "Dependency is null");
		return *s_renderDataManager;
	}


	inline gr::RenderDataManager& RenderCommand::GetRenderDataManagerForModification()
	{
		SEAssert(s_renderDataManager, "Dependency is null");
		return *s_renderDataManager;
	}


	inline std::vector<std::unique_ptr<gr::RenderSystem>> const& RenderCommand::GetRenderSystems() const
	{
		SEAssert(s_renderSystems, "Dependency is null");
		return *s_renderSystems;
	}


	inline std::vector<std::unique_ptr<gr::RenderSystem>>& RenderCommand::GetRenderSystemsForModification() const
	{
		SEAssert(s_renderSystems, "Dependency is null");
		return *s_renderSystems;
	}


	inline re::Context* RenderCommand::GetContextForModification() const
	{
		SEAssert(s_context, "Dependency is null");
		return s_context;
	}
}