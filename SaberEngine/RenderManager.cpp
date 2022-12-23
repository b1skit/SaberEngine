#include "Batch.h"
#include "Config.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_Tonemapping.h"
#include "PerformanceTimer.h"
#include "RenderManager.h"
#include "RenderManager_Platform.h"
#include "SceneManager.h"


namespace re
{
	using re::TextureTargetSet;
	using gr::GBufferGraphicsSystem;
	using gr::DeferredLightingGraphicsSystem;
	using gr::GraphicsSystem;
	using gr::ShadowsGraphicsSystem;
	using gr::SkyboxGraphicsSystem;
	using gr::BloomGraphicsSystem;
	using gr::TonemappingGraphicsSystem;
	using gr::Transform;
	using re::MeshPrimitive;
	using re::Batch;
	using en::Config;
	using en::SceneManager;
	using util::PerformanceTimer;
	using std::shared_ptr;
	using std::make_shared;
	using std::string;
	using std::vector;
	using glm::mat4;


	RenderManager* RenderManager::Get()
	{
		static std::unique_ptr<re::RenderManager> instance = std::make_unique<re::RenderManager>();
		return instance.get();
	}


	RenderManager::RenderManager() :
		m_defaultTargetSet(nullptr),
		m_pipeline("Main pipeline")
	{
	}


	RenderManager::~RenderManager()
	{
		m_pipeline.Destroy();
		m_graphicsSystems.clear();

		m_defaultTargetSet = nullptr;

		// NOTE: We must destroy anything that holds a parameter block before the ParameterBlockAllocator is destroyed, 
		// as parameter blocks call the ParameterBlockAllocator in their destructor
		m_paramBlockAllocator.Destroy();

		// Do this in the destructor so we can still read any final OpenGL error messages before it is destroyed
		m_context.Destroy();
	}


	void RenderManager::Startup()
	{
		LOG("RenderManager starting...");
		m_context.Create();

		// Default target set:
		LOG("Creating default texure target set");
		m_defaultTargetSet = make_shared<TextureTargetSet>("Default target");
		m_defaultTargetSet->Viewport() = 
		{ 
			0, 
			0, 
			(uint32_t)Config::Get()->GetValue<int>("windowXRes"),
			(uint32_t)Config::Get()->GetValue<int>("windowYRes")
		};
		// Note: Default framebuffer has no texture targets
	}


	void RenderManager::Shutdown()
	{
		LOG("Render manager shutting down...");
	}


	void RenderManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		CopyFrameData();

		// Update the graphics systems:
		for (size_t gs = 0; gs < m_graphicsSystems.size(); gs++)
		{
			m_graphicsSystems[gs]->PreRender(m_pipeline.GetPipeline()[gs]);
		}

		m_paramBlockAllocator.SwapBuffers(frameNum);

		// TODO: unblock waiting threads here before proceeding

		// Update/buffer param blocks:
		m_paramBlockAllocator.BufferParamBlocks();

		// API-specific rendering loop:
		platform::RenderManager::Render(*this);
		platform::RenderManager::RenderImGui(*this);

		// Present the final frame:
		m_context.SwapWindow();

		EndOfFrame();
	}


	void RenderManager::CopyFrameData()
	{
		SEAssert("Render batches should be empty", m_renderBatches.empty());
		m_renderBatches = std::move(SceneManager::Get()->GetSceneBatches());
	}


	void RenderManager::EndOfFrame()
	{
		m_renderBatches.clear();

		for (StagePipeline& stagePipeline : m_pipeline.GetPipeline())
		{
			stagePipeline.EndOfFrame();
		}
		
		m_paramBlockAllocator.EndOfFrame();
	}


	void RenderManager::Initialize()
	{
		LOG("RenderManager Initializing...");
		PerformanceTimer timer;
		timer.Start();

		platform::RenderManager::Initialize(*this);

		m_paramBlockAllocator.ClosePermanentPBRegistrationPeriod();

		LOG("\nRenderManager::Initialize complete in %f seconds...\n", timer.StopSec());
	}


	void RenderManager::EnqueueImGuiCommand(std::shared_ptr<en::Command> command)
	{
		m_imGuiCommands.emplace(command);
	}
}


