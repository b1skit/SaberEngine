#pragma once

#include <memory>
#include <vector>

#include "EngineComponent.h"
#include "Context.h"
#include "TextureTarget.h"
#include "RenderPipeline.h"
#include "ParameterBlockAllocator.h"


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

		void StartOfFrame(); // Starts the ImGui frame

		re::Context const& GetContext() { return m_context; }

		std::shared_ptr<gr::TextureTargetSet> GetDefaultTextureTargetSet() { return m_defaultTargetSet; }

		template <typename T>
		std::shared_ptr<gr::GraphicsSystem> GetGraphicsSystem();
		
		inline re::ParameterBlockAllocator& GetParameterBlockAllocator() { return m_paramBlockManager; }
		inline re::ParameterBlockAllocator const& GetParameterBlockAllocator() const { return m_paramBlockManager; }

		inline std::vector<re::Batch> const& GetSceneBatches() { return m_sceneBatches; }


	private:
		void BuildSceneBatches();


	private:	
		re::Context m_context;
		std::vector<std::shared_ptr<gr::GraphicsSystem>> m_graphicsSystems;
		re::RenderPipeline m_pipeline;

		// Note: We store this as a shared_ptr so we can instantiate it once the context has been created
		std::shared_ptr<gr::TextureTargetSet> m_defaultTargetSet; // Default backbuffer

		std::vector<re::Batch> m_sceneBatches;

		re::ParameterBlockAllocator m_paramBlockManager;	


	private:
		// Friends
		friend class opengl::RenderManager;

		RenderManager(RenderManager const&) = delete;
		RenderManager(RenderManager&&) = delete;
		void operator=(RenderManager const&) = delete;
	};
}


