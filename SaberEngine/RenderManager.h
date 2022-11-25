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
		void Update(const double stepTimeMs) override;
		
		// Member functions:
		void Initialize();

		re::Context const& GetContext() { return m_context; }

		std::shared_ptr<re::TextureTargetSet> GetDefaultTextureTargetSet() { return m_defaultTargetSet; }

		template <typename T>
		std::shared_ptr<gr::GraphicsSystem> GetGraphicsSystem();
		
		inline re::ParameterBlockAllocator& GetParameterBlockAllocator() { return m_paramBlockManager; }
		inline re::ParameterBlockAllocator const& GetParameterBlockAllocator() const { return m_paramBlockManager; }

		inline std::vector<re::Batch> const& GetSceneBatches() { return m_sceneBatches; }

		void EnqueueImGuiCommand(std::shared_ptr<en::Command> command);

	private:
		void BuildSceneBatches();


	private:	
		re::Context m_context;
		std::vector<std::shared_ptr<gr::GraphicsSystem>> m_graphicsSystems;
		re::RenderPipeline m_pipeline;

		// Note: We store this as a shared_ptr so we can instantiate it once the context has been created
		std::shared_ptr<re::TextureTargetSet> m_defaultTargetSet; // Default backbuffer

		std::vector<re::Batch> m_sceneBatches;

		re::ParameterBlockAllocator m_paramBlockManager;	

		std::queue<std::shared_ptr<en::Command>> m_imGuiCommands;

	private:
		// Friends
		friend class opengl::RenderManager;

		RenderManager(RenderManager const&) = delete;
		RenderManager(RenderManager&&) = delete;
		void operator=(RenderManager const&) = delete;
	};
}


