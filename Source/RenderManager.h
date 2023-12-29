// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Context.h"
#include "DoubleBufferUnorderedMap.h"
#include "EngineComponent.h"
#include "EngineThread.h"
#include "EventListener.h"
#include "NBufferedVector.h"
#include "CommandQueue.h"
#include "RenderPipeline.h"
#include "RenderSystem.h"
#include "TextureTarget.h"
#include "VertexStream.h"


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
	class Batch;
	class RenderPipeline;
}


namespace re
{
	class RenderManager
		: public virtual en::EngineComponent, public virtual en::EngineThread, public virtual en::EventListener
	{
	public:
		static RenderManager* Get(); // Singleton functionality


	public: // Platform wrappers:
		static constexpr uint8_t GetNumFrames(); // Number of buffers/frames in flight


	public:
		virtual ~RenderManager() = default;

		// EngineThread interface:
		void Lifetime(std::barrier<>* copyBarrier) override;

		inline std::vector<re::Batch> const& GetSceneBatches() { return m_renderBatches; }

		// EventListener interface:
		void HandleEvents() override;

		uint64_t GetCurrentRenderFrameNum() const;

		std::vector<std::unique_ptr<re::RenderSystem>> const& GetRenderSystems() const;



		// ECS_CONVERSION TODO: Handle this per-GS; For now just moving this out of the SceneManager
	private:
		void BuildSceneBatches();



	public: // Render commands:
		template<typename T, typename... Args>
		void EnqueueRenderCommand(Args&&... args);

	private:
		static constexpr size_t k_renderCommandBufferSize = 16 * 1024 * 1024;
		en::CommandManager m_renderCommandManager;


	public: // Deferred API-object creation queues
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
		util::NBufferedVector<std::shared_ptr<re::ParameterBlock>> m_newParameterBlocks;

		// All textures seen during CreateAPIResources(). We can't use m_newTextures, as it's cleared during Initialize()
		// Used as a holding ground for operations that must be performed once after creation (E.g. mip generation)
		std::vector<std::shared_ptr<re::Texture>> m_createdTextures;


	public: // Ensure the lifetime of single-frame resources that are referenced by in-flight batches
		template<typename T>
		void RegisterSingleFrameResource(std::shared_ptr<T>);

	private:
		util::NBufferedVector<std::shared_ptr<re::VertexStream>> m_singleFrameVertexStreams;


	private:
		// EngineComponent interface:
		void Update(uint64_t frameNum, double stepTimeMs) override;
		void Startup() override;
		void Shutdown() override;
		
		// Member functions:
		void Initialize();
		void RenderImGui();

		virtual void Render() = 0;

		void PreUpdate(uint64_t frameNum); // Synchronization step: Copies data, swaps buffers etc
		void EndOfFrame();

		
	private:
		void ShowRenderDebugImGuiWindows(bool* show);


	private:
		std::vector<std::unique_ptr<re::RenderSystem>> m_renderSystems;

		std::vector<re::Batch> m_renderBatches; // Union of all batches created by all systems. Populated in PreUpdate

		bool m_vsyncEnabled;
		
		uint64_t m_renderFrameNum;

		bool m_imguiMenuVisible;


	private:
		RenderManager(); // Use the RenderManager::Get() singleton getter instead
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


	inline uint64_t RenderManager::GetCurrentRenderFrameNum() const
	{
		return m_renderFrameNum;
	}


	inline std::vector<std::unique_ptr<re::RenderSystem>> const& RenderManager::GetRenderSystems() const
	{
		return m_renderSystems;
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