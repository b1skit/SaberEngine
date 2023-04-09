// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Command.h"
#include "Context.h"
#include "EngineComponent.h"
#include "EngineThread.h"
#include "EventListener.h"
#include "RenderPipeline.h"
#include "TextureTarget.h"


namespace opengl
{
	class RenderManager;
}

namespace dx12
{
	class RenderManager;
}

namespace gr
{
	class GraphicsSystem;
}

namespace re
{
	class Batch;
	class RenderPipeline;
}


namespace re
{
	class RenderManager final 
		: public virtual en::EngineComponent, public virtual en::EngineThread, public virtual en::EventListener
	{
	public:
		static RenderManager* Get(); // Singleton functionality

	public:
		RenderManager();
		~RenderManager() = default;

		// EngineThread interface:
		void Lifetime(std::barrier<>* copyBarrier) override;

		// Member functions:
		re::Context& GetContext() { return m_context; }
		re::Context const& GetContext() const { return m_context; }

		template <typename T>
		std::shared_ptr<gr::GraphicsSystem> GetGraphicsSystem();

		inline std::vector<re::Batch> const& GetSceneBatches() { return m_renderBatches; }

		void EnqueueImGuiCommand(std::shared_ptr<en::Command> command);

		// EventListener interface:
		void HandleEvents() override;


	private:
		// EngineComponent interface:
		void Update(uint64_t frameNum, double stepTimeMs) override;
		void Startup() override;
		void Shutdown() override;
		
		// Member functions:
		void Initialize();
		void PreUpdate(uint64_t frameNum); // Synchronization step: Copies data, swaps buffers etc
		void EndOfFrame();


	private:	
		re::Context m_context;
		std::vector<std::shared_ptr<gr::GraphicsSystem>> m_graphicsSystems;
		re::RenderPipeline m_pipeline;

		std::vector<re::Batch> m_renderBatches; // Union of all batches created by all systems. Populated in CopyFrameData

		std::queue<std::shared_ptr<en::Command>> m_imGuiCommands;

		bool m_vsyncEnabled;

	private: // Friends		
		friend class opengl::RenderManager;
		friend class dx12::RenderManager;

	private:
		RenderManager(RenderManager const&) = delete;
		RenderManager(RenderManager&&) = delete;
		void operator=(RenderManager const&) = delete;
	};


	template <typename T>
	std::shared_ptr<gr::GraphicsSystem> RenderManager::GetGraphicsSystem()
	{
		// TODO: A linear search isn't optimal here, but there aren't many graphics systems in practice so ok for now
		for (size_t i = 0; i < m_graphicsSystems.size(); i++)
		{
			if (dynamic_cast<T*>(m_graphicsSystems[i].get()) != nullptr)
			{
				return m_graphicsSystems[i];
			}
		}

		SEAssertF("Graphics system not found");
		return nullptr;
	}
}


