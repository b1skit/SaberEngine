#include <string>
#include <unordered_map>


#include "RenderManager.h"
#include "RenderManager_Platform.h"
#include "CoreEngine.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_Tonemapping.h"

using gr::TextureTargetSet;
using gr::GBufferGraphicsSystem;
using gr::DeferredLightingGraphicsSystem;
using gr::GraphicsSystem;
using gr::ShadowsGraphicsSystem;
using gr::SkyboxGraphicsSystem;
using gr::BloomGraphicsSystem;
using gr::TonemappingGraphicsSystem;
using en::CoreEngine;
using std::shared_ptr;
using std::make_shared;
using std::string;



namespace re
{
	RenderManager::RenderManager() : en::EngineComponent("RenderManager"),
		m_defaultTargetSet(nullptr)
	{
	}


	RenderManager::~RenderManager()
	{
		// Do this in the destructor so we can still read any final OpenGL error messages before it is destroyed
		m_context.Destroy();
	}


	void RenderManager::Startup()
	{
		LOG("RenderManager starting...");

		m_context.Create();

		// Default target set:
		m_defaultTargetSet = make_shared<TextureTargetSet>("Default target");
		m_defaultTargetSet->Viewport() = 
		{ 
			0, 
			0, 
			(uint32_t)CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes"),
			(uint32_t)CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes")
		};
		m_defaultTargetSet->CreateColorTargets(); // Default framebuffer has no texture targets
	}


	void RenderManager::Shutdown()
	{
		LOG("Render manager shutting down...");
	}


	void RenderManager::Update()
	{
		platform::RenderManager::Render(*this);
	}


	void RenderManager::Initialize()
	{
		platform::RenderManager::Initialize(*this);
	}


	template <typename T>
	shared_ptr<GraphicsSystem> RenderManager::GetGraphicsSystem()
	{
		// TODO: A linear search isn't optimal here, but there aren't many graphics systems in practice so ok for now
		for (size_t i = 0; i < m_graphicsSystems.size(); i++)
		{
			if (dynamic_cast<T*>(m_graphicsSystems[i].get()) != nullptr)
			{
				return m_graphicsSystems[i];
			}
		}

		SEAssert("Graphics system not found", false);
		return nullptr;
	}
	// Explicitely instantiate our templates so the compiler can link them from the .cpp file:
	template shared_ptr<GraphicsSystem> RenderManager::GetGraphicsSystem<GBufferGraphicsSystem>();
	template shared_ptr<GraphicsSystem> RenderManager::GetGraphicsSystem<DeferredLightingGraphicsSystem>();
	template shared_ptr<GraphicsSystem> RenderManager::GetGraphicsSystem<ShadowsGraphicsSystem>();
	template shared_ptr<GraphicsSystem> RenderManager::GetGraphicsSystem<SkyboxGraphicsSystem>();
	template shared_ptr<GraphicsSystem> RenderManager::GetGraphicsSystem<BloomGraphicsSystem>();
	template shared_ptr<GraphicsSystem> RenderManager::GetGraphicsSystem<TonemappingGraphicsSystem>();
}


