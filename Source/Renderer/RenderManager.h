// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "BatchPool.h"
#include "Context.h"
#include "EffectDB.h"
#include "RenderDataManager.h"
#include "RenderSystem.h"

#include "Core/CommandQueue.h"

#include "Core/Interfaces/IEngineComponent.h"
#include "Core/Interfaces/IEngineThread.h"
#include "Core/Interfaces/IEventListener.h"


namespace host
{
	class Window;
}
namespace core
{
	class Inventory;
}
namespace pr
{
	class IGraphicsService;
}
namespace re
{
	class Texture;
}
namespace gr
{
	class GraphicsSystem;


	class RenderManager
		: public virtual en::IEngineComponent, public virtual en::IEngineThread, public virtual core::IEventListener
	{
	public:
		[[nodiscard]] static std::unique_ptr<gr::RenderManager> Create();


	public:
		virtual ~RenderManager() = default;

		// IEngineThread interface:
		void Lifetime(std::barrier<>* copyBarrier) override;

		// IEventListener interface:
		void HandleEvents() override;


	protected:
		uint64_t GetCurrentRenderFrameNum() const;


		// Platform-specific virtual interface:
	private:
		virtual void Initialize_Platform() = 0;
		virtual void Shutdown_Platform() = 0;
		virtual void BeginFrame_Platform(uint64_t frameNum) = 0;
		virtual void EndFrame_Platform() = 0;
		virtual uint8_t GetNumFramesInFlight_Platform() const = 0;


	protected:
		gr::RenderSystem const* CreateAddRenderSystem(std::string const& pipelineFileName);


	private:
		host::Window* m_windowCache; // Passed to the m_context at creation

	protected:
		std::unique_ptr<re::Context> m_context;

	public:
		void SetWindow(host::Window*);


	public:
		void ShowRenderSystemsImGuiWindow(bool* showRenderMgrDebug);
		void ShowGPUCapturesImGuiWindow(bool* show);
		void ShowRenderDataImGuiWindow(bool* showRenderDataDebug) const;
		void ShowIndexedBufferManagerImGuiWindow(bool* showLightMgrDebug) const;


	private:
		gr::RenderDataManager m_renderData;
		effect::EffectDB m_effectDB;
		std::unique_ptr<gr::BatchPool> m_batchPool;


	public:
		core::CommandManager* GetRenderCommandQueue() noexcept;

	private:
		static constexpr size_t k_renderCommandBufferSize = 16 * 1024 * 1024;
		core::CommandManager m_renderCommandManager;


	private:
		// IEngineComponent interface:
		void Update(uint64_t frameNum, double stepTimeMs) override;
		void Startup() override;
		void Shutdown() override;
		
		// Member functions:
		void Initialize();

		virtual void Render() = 0;

		void BeginFrame(uint64_t frameNum);
		void EndFrame();


	protected:
		platform::RenderingAPI m_renderingAPI;


	protected:
		std::vector<std::unique_ptr<gr::RenderSystem>> m_renderSystems;
		

	private:
		uint64_t m_renderFrameNum;

		bool m_quitEventRecieved; // Early-out on final frame(s)


	protected:
		RenderManager() = delete;
		RenderManager(platform::RenderingAPI);


	private:
		RenderManager(RenderManager const&) = delete;
		RenderManager(RenderManager&&) noexcept = delete;
		void operator=(RenderManager const&) = delete;
		RenderManager& operator=(RenderManager&&) noexcept = delete;
	};


	inline uint64_t RenderManager::GetCurrentRenderFrameNum() const
	{
		return m_renderFrameNum;
	}


	inline void RenderManager::SetWindow(host::Window* window)
	{
		SEAssert(window != nullptr, "Trying to set a null window. This is unexpected");
		m_windowCache = window; // Cache this to pass to the context
	}


	inline core::CommandManager* RenderManager::GetRenderCommandQueue() noexcept
	{
		return &m_renderCommandManager;
	}
}