// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "BatchManager.h"
#include "EffectDB.h"
#include "Platform.h"
#include "RenderSystem.h"

#include "Core\CommandQueue.h"

#include "Core\Interfaces\IEngineComponent.h"
#include "Core\Interfaces\IEngineThread.h"
#include "Core\Interfaces\IEventListener.h"

#include "Core\Util\ImGuiUtils.h"
#include "Core\Util\NBufferedVector.h"


namespace opengl
{
	class RenderManager;
}

namespace dx12
{
	class RenderManager;
}

namespace gr
{
	class GraphicsSystem;
}

namespace re
{
	class TextureTarget;
	class VertexStream;
}

namespace re
{
	class RenderManager
		: public virtual en::IEngineComponent, public virtual en::IEngineThread, public virtual core::IEventListener
	{
	public:
		static RenderManager* Get(); // Singleton functionality


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

		re::RenderSystem const* CreateAddRenderSystem(std::string const& name, std::string const& pipelineFileName);
		std::vector<std::unique_ptr<re::RenderSystem>> const& GetRenderSystems() const;
		re::RenderSystem* GetRenderSystem(NameID);

		// Not thread safe: Can only be called when other threads are not accessing the render data
		gr::RenderDataManager& GetRenderDataManagerForModification();
		gr::RenderDataManager const& GetRenderDataManager() const;

		gr::BatchManager const& GetBatchManager() const;

		effect::EffectDB const& GetEffectDB() const;


	public:
		void ShowRenderSystemsImGuiWindow(bool* showRenderMgrDebug);
		void ShowGPUCapturesImGuiWindow(bool* show);
		void ShowRenderDataImGuiWindow(bool* showRenderDataDebug) const;


	private:
		gr::RenderDataManager m_renderData;
		gr::BatchManager m_batchManager;
		effect::EffectDB m_effectDB;


	public: // Render commands:
		template<typename T, typename... Args>
		void EnqueueRenderCommand(Args&&... args);


	private:
		static constexpr size_t k_renderCommandBufferSize = 16 * 1024 * 1024;
		core::CommandManager m_renderCommandManager;


	public:
		// Deferred API-object creation queues. New resources can be constructed on other threads (e.g. loading
		// data); We provide a thread-safe registration system that allows us to create the graphics API-side
		// representations at the beginning of a new frame when they're needed
		template<typename T>
		void RegisterForCreate(std::shared_ptr<T>);

		// Textures seen during CreateAPIResources() for the current frame:
		std::vector<std::shared_ptr<re::Texture>> const& GetNewlyCreatedTextures() const;

	private: // API resource management:
		void CreateAPIResources();

		void SwapNewResourceDoubleBuffers();
		void DestroyNewResourceDoubleBuffers();

		static constexpr size_t k_newObjectReserveAmount = 128;
		util::NBufferedVector<std::shared_ptr<re::Shader>> m_newShaders;
		util::NBufferedVector<std::shared_ptr<re::VertexStream>> m_newVertexStreams;
		util::NBufferedVector<std::shared_ptr<re::Texture>> m_newTextures;
		util::NBufferedVector<std::shared_ptr<re::Sampler>> m_newSamplers;
		util::NBufferedVector<std::shared_ptr<re::TextureTargetSet>> m_newTargetSets;
		util::NBufferedVector<std::shared_ptr<re::Buffer>> m_newBuffers;

		// All textures seen during CreateAPIResources(). We can't use m_newTextures, as it's cleared during Initialize()
		// Used as a holding ground for operations that must be performed once after creation (E.g. mip generation)
		std::vector<std::shared_ptr<re::Texture>> m_createdTextures;


	public: // Ensure the lifetime of single-frame resources that are referenced by in-flight batches
		template<typename T>
		void RegisterSingleFrameResource(std::shared_ptr<T>);

	private:
		util::NBufferedVector<std::shared_ptr<re::VertexStream>> m_singleFrameVertexStreams;


	private:
		// IEngineComponent interface:
		void Update(uint64_t frameNum, double stepTimeMs) override;
		void Startup() override;
		void Shutdown() override;
		
		// Member functions:
		void Initialize();

		virtual void Render() = 0;

		void PreUpdate(uint64_t frameNum); // Synchronization step: Copies data, swaps buffers etc
		void EndOfFrame();


	protected:
		platform::RenderingAPI m_renderingAPI;

	private:
		std::vector<std::unique_ptr<re::RenderSystem>> m_renderSystems;

		bool m_vsyncEnabled;
		
		uint64_t m_renderFrameNum;


	private:
		RenderManager() = delete; // Use the RenderManager::Get() singleton getter instead
		RenderManager(platform::RenderingAPI);
		[[nodiscard]] static std::unique_ptr<re::RenderManager> Create();


	private: // Friends		
		friend class opengl::RenderManager;
		friend class dx12::RenderManager;


	private:
		RenderManager(RenderManager const&) = delete;
		RenderManager(RenderManager&&) = delete;
		void operator=(RenderManager const&) = delete;
		RenderManager& operator=(RenderManager&&) = delete;
	};

	
	inline platform::RenderingAPI RenderManager::GetRenderingAPI() const
	{
		return m_renderingAPI;
	}

	inline uint64_t RenderManager::GetCurrentRenderFrameNum() const
	{
		return m_renderFrameNum;
	}


	inline std::vector<std::unique_ptr<re::RenderSystem>> const& RenderManager::GetRenderSystems() const
	{
		return m_renderSystems;
	}


	inline re::RenderSystem* RenderManager::GetRenderSystem(NameID nameID)
	{
		for (auto& renderSystem : m_renderSystems)
		{
			if (renderSystem->GetNameID() == nameID)
			{
				return renderSystem.get();
			}
		}
		SEAssertF("Failed to find render system with the given nameID. This is unexpected");
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


	inline gr::BatchManager const& RenderManager::GetBatchManager() const
	{
		return m_batchManager;
	}


	inline effect::EffectDB const& RenderManager::GetEffectDB() const
	{
		return m_effectDB;
	}


	template<typename T, typename... Args>
	inline void RenderManager::EnqueueRenderCommand(Args&&... args)
	{
		m_renderCommandManager.Enqueue<T>(std::forward<Args>(args)...);
	}


	inline std::vector<std::shared_ptr<re::Texture>> const& RenderManager::GetNewlyCreatedTextures() const
	{
		return m_createdTextures;
	}
}