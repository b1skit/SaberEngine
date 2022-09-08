#pragma once

#include <memory>
#include <vector>

#include "EngineComponent.h"
#include "TextureTarget.h"
#include "Context.h"
#include "Context_Platform.h"
#include "RenderPipeline.h"
#include "GraphicsSystem.h"


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
	class RenderManager : public virtual SaberEngine::EngineComponent
	{
	public:
		RenderManager();
		~RenderManager();

		RenderManager(RenderManager const&) = delete;
		RenderManager(RenderManager&&) = delete;
		void operator=(RenderManager const&) = delete;

		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update() override;

		
		// Member functions:
		void Initialize();

		re::Context const& GetContext() { return m_context; }

		std::shared_ptr<gr::TextureTargetSet> GetDefaultTextureTargetSet() { return m_defaultTargetSet; }

		template <typename T>
		std::shared_ptr<gr::GraphicsSystem> GetGraphicsSystem();
		

	private:	
		re::Context m_context;
		std::vector<std::shared_ptr<gr::GraphicsSystem>> m_graphicsSystems;
		re::RenderPipeline m_pipeline;

		// Note: We store this as a shared_ptr so we can instantiate it once the context has been created
		std::shared_ptr<gr::TextureTargetSet> m_defaultTargetSet; // Default backbuffer


		// Friends
		friend class opengl::RenderManager;
	};
}


