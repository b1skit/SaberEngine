// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "BindlessResourceManager.h"
#include "BufferAllocator.h"
#include "GPUTimer.h"

#include "RLibrary_Platform.h"
#include "SwapChain.h"

#include "Core/Host/Window.h"

#include "Core/Util/NBufferedVector.h"

#include "renderdoc_app.h"


namespace gr
{
	class Stage;
}
namespace opengl
{
	class Context;
}
namespace dx12
{
	class Context;
}
namespace re
{
	class AccelerationStructure;
	class Shader;
	class Texture;
	class TextureTargetSet;
	class Sampler;
	class ShaderBindingTable;
	class VertexStream;


	class Context
	{
	public:
		static constexpr char const* k_GPUFrameTimerName = "GPU Frame";


	public:
		static std::unique_ptr<re::Context> CreateContext_Platform(
			platform::RenderingAPI, uint64_t currentFrameNum, uint8_t numFramesInFlight, host::Window*);
		
		template <typename T>
		T As();


	public:
		virtual ~Context() = default;

		void Create(uint64_t currentFrame);
		void BeginFrame(uint64_t currentFrame);
		void Update();
		void EndFrame();

		void Destroy();


	public: // Context interface:
		virtual void Present() = 0;

		virtual re::BindlessResourceManager* GetBindlessResourceManager() = 0;
	private:
		virtual void CreateInternal() = 0;
		virtual void UpdateInternal() = 0;
		virtual void DestroyInternal() = 0;

		virtual void CreateAPIResources_Platform() = 0;


	public:
		host::Window* GetWindow() const noexcept;

		re::SwapChain& GetSwapChain();
		re::SwapChain const& GetSwapChain() const noexcept;

		re::BufferAllocator* GetBufferAllocator();
		re::BufferAllocator const* GetBufferAllocator() const;

		platform::RLibrary* GetOrCreateRenderLibrary(platform::RLibrary::Type);

		re::GPUTimer& GetGPUTimer();


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


	public: // API resource management:
		void CreateAPIResources();
		void ClearNewObjectCache();


	protected:
		void SwapNewResourceDoubleBuffers();
		void DestroyNewResourceDoubleBuffers();

		static constexpr size_t k_newObjectReserveAmount = 128;
		util::NBufferedVector<core::InvPtr<re::Shader>> m_newShaders;
		util::NBufferedVector<core::InvPtr<re::Texture>> m_newTextures;
		util::NBufferedVector<core::InvPtr<re::Sampler>> m_newSamplers;
		util::NBufferedVector<core::InvPtr<re::VertexStream>> m_newVertexStreams;

		util::NBufferedVector<std::shared_ptr<re::AccelerationStructure>> m_newAccelerationStructures;
		util::NBufferedVector<std::shared_ptr<re::ShaderBindingTable>> m_newShaderBindingTables;
		util::NBufferedVector<std::shared_ptr<re::TextureTargetSet>> m_newTargetSets;

		// All textures seen during CreateAPIResources(). We can't use m_newTextures, as it's cleared during Initialize()
		// Used as a holding ground for operations that must be performed once after creation (E.g. mip generation)
		std::vector<core::InvPtr<re::Texture>> m_createdTextures;


	protected:
		Context(platform::RenderingAPI api, uint8_t numFramesInFlight, host::Window*);


	protected:
		std::unique_ptr<re::BufferAllocator> m_bufferAllocator;
		std::array<std::unique_ptr<platform::RLibrary>, platform::RLibrary::Type_Count> m_renderLibraries;

	private:
		host::Window* m_window;
		re::SwapChain m_swapChain;


	protected:
		re::GPUTimer m_gpuTimer;

		uint64_t m_currentFrameNum;
		uint8_t m_numFramesInFlight;


	public: // RenderDoc debugging
		typedef RENDERDOC_API_1_1_2 RenderDocAPI;
		RenderDocAPI* GetRenderDocAPI() const;

	private:
		RenderDocAPI* m_renderDocApi;
	};


	inline host::Window* Context::GetWindow() const noexcept
	{
		return m_window;
	}


	inline re::SwapChain& Context::GetSwapChain()
	{
		return m_swapChain;
	}


	inline re::SwapChain const& Context::GetSwapChain() const noexcept
	{
		return m_swapChain;
	}


	inline re::BufferAllocator* Context::GetBufferAllocator()
	{
		SEAssert(m_bufferAllocator && m_bufferAllocator->IsValid(), "Buffer allocator has already been destroyed");
		return m_bufferAllocator.get();
	}


	inline re::BufferAllocator const* Context::GetBufferAllocator() const
	{
		SEAssert(m_bufferAllocator && m_bufferAllocator->IsValid(), "Buffer allocator has already been destroyed");
		return m_bufferAllocator.get();
	}


	inline re::GPUTimer& Context::GetGPUTimer()
	{
		return m_gpuTimer;
	}


	template <typename T>
	inline T Context::As()
	{
		return dynamic_cast<T>(this);
	}


	inline Context::RenderDocAPI* Context::GetRenderDocAPI() const
	{
		return m_renderDocApi;
	}
}