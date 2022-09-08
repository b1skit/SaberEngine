#pragma once

#include <string>
#include <memory>
#include <vector>

#include <GL/glew.h>
#include <SDL.h>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include "EngineComponent.h"
#include "Mesh.h"
#include "TextureTarget.h"
#include "Context.h"
#include "Context_Platform.h"
#include "RenderPipeline.h"
#include "GraphicsSystem.h"


namespace gr
{
	class Mesh;
	class Shader;
	class Camera;
	class Light;
	class GraphicsSystem;
}

namespace SaberEngine
{
	class RenderManager : public virtual EngineComponent
	{
	public:
		RenderManager() : EngineComponent("RenderManager") {};
		~RenderManager();

		RenderManager(RenderManager const&) = delete; // Disallow copying of our Singleton
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
		void Render();
		
		re::Context m_context;

		// Note: We store this as a shared_ptr so we can instantiate it once the context has been created
		std::shared_ptr<gr::TextureTargetSet> m_defaultTargetSet = nullptr; // Default backbuffer

		std::vector<std::shared_ptr<gr::GraphicsSystem>> m_graphicsSystems;
		re::RenderPipeline m_pipeline;

		// TODO: Move initialization to ctor init list
	};
}


