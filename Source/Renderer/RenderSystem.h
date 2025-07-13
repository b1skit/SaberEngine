// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystemManager.h"
#include "GraphicsSystem.h"
#include "RenderCommand.h"
#include "RenderPipeline.h"

#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IUniqueID.h"


namespace core
{
	class Inventory;
}
namespace effect
{
	class EffectDB;
}
namespace re
{
	class Context;
}
namespace gr
{
	class RenderDataManager;
	struct RenderSystemDescription;
	class IndexedBufferManager;


	class RenderSystem : public virtual core::INamedObject, public virtual core::IUniqueID
	{
	public:
		[[nodiscard]] static std::unique_ptr<RenderSystem> Create(
			std::string const& pipelineFileName, RenderDataManager const*, re::Context*);
		
		void Destroy();

		~RenderSystem() { Destroy(); };

		RenderSystem(RenderSystem&&) noexcept = default;
		RenderSystem& operator=(RenderSystem&&) noexcept = default;


	public:
		// Scriptable rendering pipeline:
		void BuildPipeline(gr::RenderSystemDescription const&, gr::RenderDataManager const*); // Creates graphics systems + init/update pipelines

	private:
		void ExecuteInitializationPipeline();

	public:
		void ExecuteUpdatePipeline(uint64_t currentFrameNum);


	public:
		void PostUpdatePreRender(IndexedBufferManager&, effect::EffectDB const&); // Called after ExecuteUpdatePipeline()
		void EndOfFrame();


	public:
		gr::GraphicsSystemManager& GetGraphicsSystemManager();
		gr::GraphicsSystemManager const& GetGraphicsSystemManager() const;

		gr::RenderPipeline& GetRenderPipeline();

		void ShowImGuiWindow();


	private:
		gr::GraphicsSystemManager m_graphicsSystemManager;

		gr::RenderPipeline m_renderPipeline;
		std::function<void(gr::RenderSystem*)> m_creationPipeline;
		std::function<void(gr::RenderSystem*)> m_initPipeline;

	private:
		struct UpdateStep
		{
			gr::GraphicsSystem::RuntimeBindings::PreRenderFn m_preRenderFunc;
			
			// Convenience for debugging/logging:
			gr::GraphicsSystem const* m_gs; 
			std::string const m_scriptFunctionName;
		};

		std::vector<std::vector<UpdateStep>> m_updatePipeline;


	private: // Use the Create() factory
		RenderSystem(std::string const& name, re::Context*);
		RenderSystem() = delete; 


	private: // No copying allowed:
		RenderSystem(RenderSystem const&) = delete;
		RenderSystem& operator=(RenderSystem const&) = delete;
	};


	inline gr::GraphicsSystemManager& RenderSystem::GetGraphicsSystemManager()
	{
		return m_graphicsSystemManager;
	}


	inline gr::GraphicsSystemManager const& RenderSystem::GetGraphicsSystemManager() const
	{
		return m_graphicsSystemManager;
	}


	inline gr::RenderPipeline& RenderSystem::GetRenderPipeline()
	{
		return m_renderPipeline;
	}


	// ---


	class CreateAddRenderSystem : public virtual gr::RenderCommand
	{
	public:
		CreateAddRenderSystem(std::string const& pipelineFileName) : m_pipelineFileName(pipelineFileName) {}
		~CreateAddRenderSystem() = default;

		static void Execute(void*);

	private:
		std::string m_pipelineFileName;
	};
}

