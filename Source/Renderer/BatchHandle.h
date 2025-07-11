// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "BufferView.h"
#include "RenderObjectIDs.h"
#include "VertexStream.h"

#include "Core/InvPtr.h"

#include "Core/Util/CHashKey.h" // Required for DrawStyles.h

#include "_generated/DrawStyles.h"


namespace effect
{
	class EffectDB;
}
namespace gr
{
	class Batch;
	class BatchPool;
}
namespace re
{
	class Shader;
	class Buffer;
}

namespace gr
{
	using PoolIndex = uint32_t;
	static constexpr PoolIndex k_invalidPoolIndex = std::numeric_limits<PoolIndex>::max();


	class BatchHandle final
	{
	public:
		BatchHandle() noexcept // Invalid handle
			: m_poolIndex(k_invalidPoolIndex)
			, m_renderDataID(gr::k_invalidRenderDataID)
		{}


		BatchHandle(PoolIndex batchIndex, gr::RenderDataID renderDataID) noexcept;


	public:
		BatchHandle(BatchHandle&&) noexcept;
		BatchHandle& operator=(BatchHandle&&) noexcept;
		BatchHandle(BatchHandle const&) noexcept;
		BatchHandle& operator=(BatchHandle const&) noexcept;
		~BatchHandle() noexcept;


	public:
		gr::Batch const* operator->() const noexcept;
		gr::Batch const& operator*() const noexcept;

		gr::PoolIndex GetPoolIndex() const noexcept;

		gr::RenderDataID GetRenderDataID() const noexcept;

		bool IsValid() const noexcept;

	private:
		gr::PoolIndex m_poolIndex; // Global index in the batch pool

		gr::RenderDataID m_renderDataID; // RenderDataID batch was created from (if any)

//#define BATCH_HANDLE_DEBUG
#if defined(BATCH_HANDLE_DEBUG)
		gr::Batch const* m_batch = nullptr;
#endif

	private:
		friend class gr::BatchPool;
		static gr::BatchPool* s_batchPool;
	};


	// ---


	class StageBatchHandle final
	{
	public:
		using ResolvedVertexBuffers =
			std::array<std::pair<re::VertexBufferInput const*, uint8_t>, re::VertexStream::k_maxVertexStreams>;

	public:
		StageBatchHandle(BatchHandle);

	public:
		BatchHandle const& operator*() const noexcept { return m_batchHandle; }


	public:
		void SetSingleFrameBuffer(std::string_view shaderName, std::shared_ptr<re::Buffer> const&);
		void SetSingleFrameBuffer(std::string_view shaderName, std::shared_ptr<re::Buffer> const&, re::BufferView const&);
		void SetSingleFrameBuffer(re::BufferInput const&);


	public:
		void Resolve(effect::drawstyle::Bitmask stageDrawstyleBits, uint32_t instanceCount, effect::EffectDB const&);


	public:
		core::InvPtr<re::Shader> const& GetShader() const;

		uint32_t GetInstanceCount() const;

		std::vector<re::BufferInput> const& GetSingleFrameBuffers() const;

		ResolvedVertexBuffers const& GetResolvedVertexBuffers() const;
		std::pair<re::VertexBufferInput const*, uint8_t> const& GetResolvedVertexBuffer(uint8_t slotIdx) const&;

		re::VertexBufferInput const& GetIndexBuffer() const;


	private:
		BatchHandle m_batchHandle;

		core::InvPtr<re::Shader> m_batchShader;

		std::vector<re::BufferInput> m_singleFrameBuffers; // E.g. Instanced buffers

		ResolvedVertexBuffers m_resolvedVertexBuffers{};

		uint32_t m_instanceCount;

		bool m_isResolved;
	};


	// ---


	inline gr::PoolIndex BatchHandle::GetPoolIndex() const noexcept
	{
		return m_poolIndex;
	}


	inline gr::RenderDataID BatchHandle::GetRenderDataID() const noexcept
	{
		return m_renderDataID;
	}


	inline bool BatchHandle::IsValid() const noexcept
	{
		return m_poolIndex != k_invalidPoolIndex;
	}
	

	// ---


	inline StageBatchHandle::StageBatchHandle(BatchHandle batchHandle)
		: m_batchHandle(batchHandle)
		, m_instanceCount(0)
		, m_isResolved(false)
	{
		for (auto& entry : m_resolvedVertexBuffers)
		{
			entry = { nullptr, re::VertexBufferInput::k_invalidSlotIdx }; // Initialize as invalid
		}
	}


	inline core::InvPtr<re::Shader> const& StageBatchHandle::GetShader() const
	{
		SEAssert(m_isResolved, "StageBatchHandle has not been resolved");
		return m_batchShader;
	}


	inline uint32_t StageBatchHandle::GetInstanceCount() const
	{
		SEAssert(m_isResolved, "StageBatchHandle has not been resolved");
		return m_instanceCount;
	}


	inline std::vector<re::BufferInput> const& StageBatchHandle::GetSingleFrameBuffers() const
	{
		SEAssert(m_isResolved, "StageBatchHandle has not been resolved");
		return m_singleFrameBuffers;
	}


	inline void StageBatchHandle::SetSingleFrameBuffer(
		std::string_view shaderName, std::shared_ptr<re::Buffer> const& buffer)
	{
		SetSingleFrameBuffer(re::BufferInput(shaderName, buffer));
	}


	inline void StageBatchHandle::SetSingleFrameBuffer(
		std::string_view shaderName, std::shared_ptr<re::Buffer> const& buffer, re::BufferView const& view)
	{
		SetSingleFrameBuffer(re::BufferInput(shaderName, buffer, view));
	}


	inline void StageBatchHandle::SetSingleFrameBuffer(re::BufferInput const& bufferInput)
	{
		m_singleFrameBuffers.push_back(bufferInput);
	}


	inline gr::StageBatchHandle::ResolvedVertexBuffers const& StageBatchHandle::GetResolvedVertexBuffers() const
	{
		SEAssert(m_isResolved, "StageBatchHandle has not been resolved");
		return m_resolvedVertexBuffers;
	}
}