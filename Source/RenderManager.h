// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "EngineComponent.h"
#include "EngineThread.h"
#include "EventListener.h"
#include "ImGuiUtils.h"
#include "NBufferedVector.h"
#include "CommandQueue.h"
#include "RenderSystem.h"


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
		: public virtual en::EngineComponent, public virtual en::EngineThread, public virtual en::EventListener
	{
	public:
		static RenderManager* Get(); // Singleton functionality


	public: // Platform wrappers:
		static uint8_t GetNumFramesInFlight();


	public:
		virtual ~RenderManager() = default;

		// EngineThread interface:
		void Lifetime(std::barrier<>* copyBarrier) override;

		// EventListener interface:
		void HandleEvents() override;

		uint64_t GetCurrentRenderFrameNum() const;

		std::vector<std::unique_ptr<re::RenderSystem>> const& GetRenderSystems() const;


	public:
		std::mutex& GetGlobalImGuiMutex(); // Synchronize ImGui IO accesses across threads


	public:
		void ShowRenderSystemsImGuiWindow(bool* showRenderMgrDebug);
		void ShowGPUCapturesImGuiWindow(bool* show);
		void ShowRenderDataImGuiWindow(bool* showRenderDataDebug) const;


	private:
		void RenderImGui(); // Process ImGui render commands

		static constexpr size_t k_imGuiCommandBufferSize = 8 * 1024 * 1024;
		en::CommandManager m_imGuiCommandManager;

		std::mutex m_imGuiGlobalMutex;

		// We will ignore ImGui commands if a quit event is received. The render thread is always a frame behind the
		// front end thread; if it's processing ImGui commands it might try and access something after it is destroyed.
		// Note: The RenderManager's lifetime is otherwise exclusively controlled by the en::EngineThread interface
		bool m_quitEventReceived; 


	public: // Render commands:
		template<typename T, typename... Args>
		void EnqueueRenderCommand(Args&&... args);

		template<typename T, typename... Args>
		void EnqueueImGuiCommand(Args&&... args);


	private:
		static constexpr size_t k_renderCommandBufferSize = 16 * 1024 * 1024;
		en::CommandManager m_renderCommandManager;


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

		virtual void Render() = 0;

		void PreUpdate(uint64_t frameNum); // Synchronization step: Copies data, swaps buffers etc
		void EndOfFrame();


	private:
		std::vector<std::unique_ptr<re::RenderSystem>> m_renderSystems;

		bool m_vsyncEnabled;
		
		uint64_t m_renderFrameNum;


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


	inline std::mutex& RenderManager::GetGlobalImGuiMutex()
	{
		return m_imGuiGlobalMutex;
	}


	template<typename T, typename... Args>
	inline void RenderManager::EnqueueRenderCommand(Args&&... args)
	{
		m_renderCommandManager.Enqueue<T>(std::forward<Args>(args)...);
	}


	template<typename T, typename... Args>
	void RenderManager::EnqueueImGuiCommand(Args&&... args)
	{
		m_imGuiCommandManager.Enqueue<T>(std::forward<Args>(args)...);
	}


	inline std::vector<std::shared_ptr<re::Texture>> const& RenderManager::GetNewlyCreatedTextures() const
	{
		return m_createdTextures;
	}


	// ---


	/*	A helper to cut down on ImGui render command boiler plate.This is not mandatory for submitting commands to the
	*	ImGui command queue, but it should cover most common cases. 
	* 
	*	Internally, it uses a static unordered_map to link a unique identifier (e.g. a lambda address or some other
	*	value) with ImGui's ImGuiOnceUponAFrame to ensure commands submitted multiple times per frame are only executed
	*	once.
	*	
	*	Use this class as a wrapper for a lambda that captures any required data by reference:
	* 
	*	auto SomeLambda = [&]() { // Do something };
	* 
	*	re::RenderManager::Get()->EnqueueImGuiCommand<re::ImGuiRenderCommand<decltype(SomeLambda)>>(
	*		re::ImGuiRenderCommand<decltype(SomeLambda)>(SomeLambda));
	* 
	*	In cases where the lamba being wrapped is called multiple times but capturing different data (e.g. from within
	*	a static function), use the 2nd ctor to supply a unique ID each time
	*/
	template<typename T>
	class ImGuiRenderCommand
	{
	public:
		// When the 
		ImGuiRenderCommand(T wrapperLambda)
			: m_imguiWrapperLambda(wrapperLambda), m_uniqueID(util::PtrToID(&wrapperLambda)) {};

		ImGuiRenderCommand(T wrapperLambda, uint64_t uniqueID)
			: m_imguiWrapperLambda(wrapperLambda), m_uniqueID(uniqueID) {};

		static void Execute(void* cmdData)
		{
			ImGuiRenderCommand<T>* cmdPtr = reinterpret_cast<ImGuiRenderCommand<T>*>(cmdData);
			if (m_uniqueIDToImGuiOnceUponAFrame[cmdPtr->m_uniqueID]) // Insert or access our ImGuiOnceUponAFrame
			{
				cmdPtr->m_imguiWrapperLambda();
			}
		}

		static void Destroy(void* cmdData)
		{
			ImGuiRenderCommand<T>* cmdPtr = reinterpret_cast<ImGuiRenderCommand<T>*>(cmdData);
			cmdPtr->~ImGuiRenderCommand();
		}

	private:
		T m_imguiWrapperLambda;
		uint64_t m_uniqueID;

		static std::unordered_map<uint64_t, ImGuiOnceUponAFrame> m_uniqueIDToImGuiOnceUponAFrame;
	};
	template<typename T>
	inline std::unordered_map<uint64_t, ImGuiOnceUponAFrame> ImGuiRenderCommand<T>::m_uniqueIDToImGuiOnceUponAFrame;
}