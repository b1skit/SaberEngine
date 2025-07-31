// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"
#include "GraphicsSystemManager.h"
#include "RenderCommand.h"
#include "RenderPipeline.h"

#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IUniqueID.h"

#include "Core/Util/CHashKey.h"


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
	class IndexedBufferManager;

	struct RenderPipelineDesc;


	class RenderSystem : public virtual core::INamedObject, public virtual core::IUniqueID
	{
	public:
		[[nodiscard]] static std::unique_ptr<RenderSystem> Create(
			std::string const& pipelineFileName, RenderDataManager const*, re::Context*);
		
		void Destroy();

		~RenderSystem() = default;

		RenderSystem(RenderSystem&&) noexcept = default;
		RenderSystem& operator=(RenderSystem&&) noexcept = default;


	private:
		// Scriptable rendering pipeline:
		void BuildPipeline(gr::RenderPipelineDesc const&, gr::RenderDataManager const*); // Creates graphics systems + init/update pipelines

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


	public:
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

		// Cache our config settings so we can clear them when the render system is destroyed. We keep the strings around
		// for debugging, but they're stored in the config as CHashKeys
		std::vector<std::pair<std::string, std::string>> m_configSettings;


	private: // Use the Create() factory
		RenderSystem(
			std::string const& name, std::vector<std::pair<std::string, std::string>> const& configSettings, re::Context*);
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

