// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Context.h"
#include "EffectDB.h"
#include "RenderSystem.h"

#include "Core/InvPtr.h"
#include "Core/CommandQueue.h"

#include "Core/Interfaces/IEngineComponent.h"
#include "Core/Interfaces/IEngineThread.h"
#include "Core/Interfaces/IEventListener.h"
#include "Core/Interfaces/IPlatformObject.h"

#include "Core/Util/NBufferedVector.h"


namespace host {
class Window;
} // namespace host

namespace core
{
	class Inventory;
}

namespace dx12
{
	class RenderManager;
}

namespace gr
{
	class GraphicsSystem;
	class VertexStream;
}

namespace opengl
{
	class RenderManager;
}

namespace re
{
	class AccelerationStructure;
	class ShaderBindingTable;


	class RenderManager
		: public virtual en::IEngineComponent, public virtual en::IEngineThread, public virtual core::IEventListener
	{
	public:
		static RenderManager* Get(); // Singleton functionality

	public:
		static constexpr char const* k_GPUFrameTimerName = "GPU Frame";


	public: // Platform wrappers:
		static uint8_t GetNumFramesInFlight();
		static float GetWindowAspectRatio();


	public:
		virtual ~RenderManager() = default;

		// IEngineThread interface:
		void Lifetime(std::barrier<>* copyBarrier) override;

		// IEventListener interface:
		void HandleEvents() override;

		platform::RenderingAPI GetRenderingAPI() const;
		uint64_t GetCurrentRenderFrameNum() const;

		gr::RenderSystem const* CreateAddRenderSystem(std::string const& pipelineFileName);
		std::vector<std::unique_ptr<gr::RenderSystem>> const& GetRenderSystems() const;
		gr::RenderSystem* GetRenderSystem(util::HashKey const&);

		// Not thread safe: Can only be called when other threads are not accessing the render data
		gr::RenderDataManager& GetRenderDataManagerForModification();
		gr::RenderDataManager const& GetRenderDataManager() const;

		effect::EffectDB const& GetEffectDB() const;


	public:
		void SetInventory(core::Inventory*); // Dependency injection: Call once immediately after creation
		core::Inventory* GetInventory() const;

		re::Context* GetContext() { return m_context.get(); }
		re::Context const* GetContext() const { return m_context.get(); }

	private:
		core::Inventory* m_inventory;
		host::Window* m_window_ptr;
		std::unique_ptr<re::Context> m_context;

	public:
		void SetWindow(host::Window* window);


	public:
		void ShowRenderSystemsImGuiWindow(bool* showRenderMgrDebug);
		void ShowGPUCapturesImGuiWindow(bool* show);
		void ShowRenderDataImGuiWindow(bool* showRenderDataDebug) const;
		void ShowIndexedBufferManagerImGuiWindow(bool* showLightMgrDebug) const;
		void ShowLightManagerImGuiWindow(bool* showLightMgrDebug) const;


	private:
		gr::RenderDataManager m_renderData;
		effect::EffectDB m_effectDB;


	public: // Render commands:
		template<typename T, typename... Args>
		void EnqueueRenderCommand(Args&&... args);

		void EnqueueRenderCommand(std::function<void(void)>&&);


	private:
		static constexpr size_t k_renderCommandBufferSize = 16 * 1024 * 1024;
		core::CommandManager m_renderCommandManager;


	public:
		// Deferred API-object creation queues. New resources can be constructed on other threads (e.g. loading
		// data); We provide a thread-safe registration system that allows us to create the graphics API-side
		// representations at the beginning of a new frame when they're needed
		template<typename T>
		void RegisterForCreate(std::shared_ptr<T> const&);

		template<typename T>
		void RegisterForCreate(core::InvPtr<T> const&);

		// Resources created during CreateAPIResources() for the current frame
		template<typename T>
		std::vector<core::InvPtr<T>> const& GetNewResources() const;


	private: // API resource management:
		void CreateAPIResources();
		void ClearNewObjectCache();

		void SwapNewResourceDoubleBuffers();
		void DestroyNewResourceDoubleBuffers();

		static constexpr size_t k_newObjectReserveAmount = 128;
		util::NBufferedVector<core::InvPtr<re::Shader>> m_newShaders;
		util::NBufferedVector<core::InvPtr<re::Texture>> m_newTextures;
		util::NBufferedVector<core::InvPtr<re::Sampler>> m_newSamplers;
		util::NBufferedVector<core::InvPtr<gr::VertexStream>> m_newVertexStreams;

		util::NBufferedVector<std::shared_ptr<re::AccelerationStructure>> m_newAccelerationStructures;
		util::NBufferedVector<std::shared_ptr<re::ShaderBindingTable>> m_newShaderBindingTables;
		util::NBufferedVector<std::shared_ptr<re::TextureTargetSet>> m_newTargetSets;		

		// All textures seen during CreateAPIResources(). We can't use m_newTextures, as it's cleared during Initialize()
		// Used as a holding ground for operations that must be performed once after creation (E.g. mip generation)
		std::vector<core::InvPtr<re::Texture>> m_createdTextures;


	private:
		void CreateSamplerLibrary();
		
		void CreateDefaultTextures();
		std::unordered_map<util::HashKey, core::InvPtr<re::Texture>> m_defaultTextures;

	public:
		core::InvPtr<re::Texture> const& GetDefaultTexture(util::HashKey);


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


	public:
		void RegisterForDeferredDelete(std::unique_ptr<core::IPlatObj>&&);

	private:
		static constexpr uint64_t k_forceDeferredDeletionsFlag = std::numeric_limits<uint64_t>::max();

		void ProcessDeferredDeletions(uint64_t frameNum);
		
		struct PlatformDeferredDelete
		{
			std::unique_ptr<core::IPlatObj> m_platObj;
			uint64_t m_frameNum; // When the delete was recorded: Delete will happen after GetNumFramesInFlight() frames
		};
		std::queue<PlatformDeferredDelete> m_deletedPlatObjects;
		std::mutex m_deletedPlatObjectsMutex;


	protected:
		platform::RenderingAPI m_renderingAPI;


	private:
		std::vector<std::unique_ptr<gr::RenderSystem>> m_renderSystems;
		
		uint64_t m_renderFrameNum;

		bool m_quitEventRecieved; // Early-out on final frame(s)


	private:
		RenderManager() = delete; // Use the RenderManager::Get() singleton getter instead
		RenderManager(platform::RenderingAPI api);
		[[nodiscard]] static std::unique_ptr<re::RenderManager> Create();


	private: // Friends		
		friend class opengl::RenderManager;
		friend class dx12::RenderManager;


	private:
		RenderManager(RenderManager const&) = delete;
		RenderManager(RenderManager&&) noexcept = delete;
		void operator=(RenderManager const&) = delete;
		RenderManager& operator=(RenderManager&&) noexcept = delete;
	};

	
	inline platform::RenderingAPI RenderManager::GetRenderingAPI() const
	{
		return m_renderingAPI;
	}

	inline uint64_t RenderManager::GetCurrentRenderFrameNum() const
	{
		return m_renderFrameNum;
	}


	inline std::vector<std::unique_ptr<gr::RenderSystem>> const& RenderManager::GetRenderSystems() const
	{
		return m_renderSystems;
	}


	inline gr::RenderSystem* RenderManager::GetRenderSystem(util::HashKey const& nameHash)
	{
		for (auto& renderSystem : m_renderSystems)
		{
			if (renderSystem->GetNameHash() == nameHash)
			{
				return renderSystem.get();
			}
		}
		SEAssertF("Failed to find render system with the given nameHash. This is unexpected");
		return nullptr;
	}


	inline gr::RenderDataManager& RenderManager::GetRenderDataManagerForModification()
	{
		return m_renderData;
	}


	inline gr::RenderDataManager const& RenderManager::GetRenderDataManager() const
	{
		return m_renderData;
	}


	inline effect::EffectDB const& RenderManager::GetEffectDB() const
	{
		return m_effectDB;
	}


	inline void RenderManager::SetInventory(core::Inventory* inventory)
	{
		m_inventory = inventory;
	}


	inline void RenderManager::SetWindow(host::Window* window)
	{
		SEAssert(window != nullptr, "Trying to set a null window. This is unexpected");
		m_window_ptr = window;
		// If context exists and needs window update: m_context->SetWindow(window);
		// However, Context::SetWindow was removed, so this is primarily for initial setup.
	}


	inline core::Inventory* RenderManager::GetInventory() const
	{
		return m_inventory;
	}


	template<typename T, typename... Args>
	inline void RenderManager::EnqueueRenderCommand(Args&&... args)
	{
		m_renderCommandManager.Enqueue<T>(std::forward<Args>(args)...);
	}


	inline void RenderManager::EnqueueRenderCommand(std::function<void(void)>&& lambda)
	{
		m_renderCommandManager.Enqueue(std::move(lambda));
	}


	inline core::InvPtr<re::Texture> const& RenderManager::GetDefaultTexture(util::HashKey texName)
	{
		SEAssert(m_defaultTextures.contains(texName), "Default texture with the given name not found");
		return m_defaultTextures.at(texName);
	}
}