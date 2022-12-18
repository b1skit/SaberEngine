#pragma once

#include <memory>
#include <vector>

#include "EngineComponent.h"
#include "Context.h"
#include "TextureTarget.h"
#include "RenderPipeline.h"
#include "ParameterBlockAllocator.h"
#include "Command.h"


namespace opengl
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
	class RenderManager : public virtual en::EngineComponent
	{
	public:
		static RenderManager* Get(); // Singleton functionality

	public:
		RenderManager();
		~RenderManager();

		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(uint64_t frameNum, double stepTimeMs) override;
		
		// Member functions:
		void Initialize();

		re::Context const& GetContext() { return m_context; }

		std::shared_ptr<re::TextureTargetSet> GetDefaultTextureTargetSet() { return m_defaultTargetSet; }

		template <typename T>
		std::shared_ptr<gr::GraphicsSystem> GetGraphicsSystem();
		
		inline re::ParameterBlockAllocator& GetParameterBlockAllocator() { return m_paramBlockAllocator; }
		inline re::ParameterBlockAllocator const& GetParameterBlockAllocator() const { return m_paramBlockAllocator; }

		inline std::vector<re::Batch> const& GetSceneBatches() { return m_renderBatches; }

		void EnqueueImGuiCommand(std::shared_ptr<en::Command> command);

	private:
		void CopyFrameData();

		void EndOfFrame();

	private:	
		re::Context m_context;
		std::vector<std::shared_ptr<gr::GraphicsSystem>> m_graphicsSystems;
		re::RenderPipeline m_pipeline;

		// Note: We store this as a shared_ptr so we can instantiate it once the context has been created
		std::shared_ptr<re::TextureTargetSet> m_defaultTargetSet; // Default backbuffer

		std::vector<re::Batch> m_renderBatches; // Union of all batches created by all systems. Populated in CopyFrameData

		re::ParameterBlockAllocator m_paramBlockAllocator;	

		std::queue<std::shared_ptr<en::Command>> m_imGuiCommands;

	private:
		// Friends
		friend class opengl::RenderManager;

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


