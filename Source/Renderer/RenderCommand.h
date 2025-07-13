// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Core/CommandQueue.h"


namespace gr
{
	class RenderManager;
	class RenderDataManager;


	class RenderCommand
	{
	public:
		template<typename T, typename... Args>
		requires std::derived_from<T, RenderCommand>
		static void Enqueue(Args&&... args);

		static void Enqueue(std::function<void(void)>&&);


	protected:
		gr::RenderDataManager& GetRenderDataManagerForModification();


	private:
		friend class RenderManager;
		static core::CommandManager* s_renderCommandManager;
		static gr::RenderDataManager* s_renderDataManager;
	};


	template<typename T, typename... Args>
	requires std::derived_from<T, RenderCommand>
	inline void RenderCommand::Enqueue(Args&&... args)
	{
		s_renderCommandManager->Enqueue<T>(std::forward<Args>(args)...);
	}


	inline void RenderCommand::Enqueue(std::function<void(void)>&& lambda)
	{
		s_renderCommandManager->Enqueue(std::forward<std::function<void(void)>>(lambda));
	}


	inline gr::RenderDataManager& RenderCommand::GetRenderDataManagerForModification()
	{
		return *s_renderDataManager;
	}
}