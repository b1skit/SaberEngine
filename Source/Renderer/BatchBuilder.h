// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Batch.h"
#include "BatchHandle.h"
#include "BatchPool.h"
#include "BufferView.h"
#include "EnumTypes.h"
#include "RenderObjectIDs.h"
#include "MeshPrimitive.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/IUniqueID.h"


namespace re
{
	class Buffer;
	class Sampler;
	class Shader;
	class Texture;
	struct TextureView;
}

namespace gr
{
	class RenderDataManager;


	template<typename BuilderImpl>
	class IBatchBuilder
	{
	protected:
		IBatchBuilder(re::Batch::BatchType) noexcept;
		IBatchBuilder(re::Batch const&) noexcept;
		
		IBatchBuilder(IBatchBuilder&&) noexcept = default;
		IBatchBuilder& operator=(IBatchBuilder&&) noexcept = default;
		virtual ~IBatchBuilder() noexcept = default;


	public:
		BuilderImpl&& SetEffectID(EffectID) && noexcept;

		BuilderImpl&& SetBuffer(std::string_view shaderName, std::shared_ptr<re::Buffer> const&)&& noexcept;
		BuilderImpl&& SetBuffer(std::string_view shaderName, std::shared_ptr<re::Buffer> const&, re::BufferView const&)&& noexcept;
		BuilderImpl&& SetBuffer(re::BufferInput&&)&& noexcept;
		BuilderImpl&& SetBuffer(re::BufferInput const&)&& noexcept;

		BuilderImpl&& SetTextureInput(
			std::string_view shaderName,
			core::InvPtr<re::Texture> const&,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&)&& noexcept;

		BuilderImpl&& SetRWTextureInput(
			std::string_view shaderName,
			core::InvPtr<re::Texture> const&,
			re::TextureView const&)&& noexcept;

		BuilderImpl&& SetRootConstant(std::string_view shaderName, void const* src, re::DataType)&& noexcept;

		BuilderImpl&& SetFilterMaskBit(re::Batch::Filter filterBit, bool enabled)&& noexcept;


	public:
		gr::BatchHandle Build() && noexcept;
		

	protected:
		re::Batch m_batch; // The batch we're building
		gr::RenderDataID m_renderDataID; // RenderDataID associated with the Batch we're building (if any)


	protected:
		friend class gr::BatchPool;
		static gr::BatchPool* s_batchPool;


	private: // R-value only:
		IBatchBuilder(IBatchBuilder const&) = delete;
		IBatchBuilder operator=(IBatchBuilder const&) = delete;
		IBatchBuilder() noexcept = delete;
	};


	template<typename BuilderImpl>
	gr::BatchPool* IBatchBuilder<BuilderImpl>::s_batchPool = nullptr;


	// ---


	class RasterBatchBuilder : public IBatchBuilder<RasterBatchBuilder>
	{
	public:
		RasterBatchBuilder() noexcept : IBatchBuilder(re::Batch::BatchType::Raster) {}


	public:
		using BuildFromRenderDataCallback = gr::RasterBatchBuilder&& (*)(
			RasterBatchBuilder&&,
			re::Batch::VertexStreamOverride const*,
			gr::RenderDataID,
			gr::RenderDataManager const&);

		static RasterBatchBuilder CreateInstance(
			gr::RenderDataID,
			gr::RenderDataManager const&,
			BuildFromRenderDataCallback,
			re::Batch::VertexStreamOverride const* = nullptr) noexcept;


		using BuildFromMeshPrimitiveCallback = 
			gr::RasterBatchBuilder&& (*)(gr::RasterBatchBuilder&&, core::InvPtr<gr::MeshPrimitive> const&, EffectID);
		
		static RasterBatchBuilder CreateMeshPrimitiveBatch(
			core::InvPtr<gr::MeshPrimitive> const&, EffectID, BuildFromMeshPrimitiveCallback);

		static RasterBatchBuilder CloneAndModify(BatchHandle) noexcept;


	public:
		RasterBatchBuilder&& SetGeometryMode(re::Batch::GeometryMode)&& noexcept;
		RasterBatchBuilder&& SetPrimitiveTopology(gr::MeshPrimitive::PrimitiveTopology)&& noexcept;

		RasterBatchBuilder&& SetVertexBuffer(uint8_t slotIdx, re::VertexBufferInput&&)&& noexcept;
		RasterBatchBuilder&& SetVertexBuffer(uint8_t slotIdx, re::VertexBufferInput const&)&& noexcept;

		RasterBatchBuilder&& SetVertexBuffers(std::array<re::VertexBufferInput, re::VertexStream::k_maxVertexStreams>&&)&& noexcept;
		RasterBatchBuilder&& SetVertexBuffers(std::array<re::VertexBufferInput, re::VertexStream::k_maxVertexStreams>const&)&& noexcept;

		RasterBatchBuilder&& SetVertexStreamOverrides(re::Batch::VertexStreamOverride const*)&& noexcept;

		RasterBatchBuilder&& SetIndexBuffer(re::VertexBufferInput&&)&& noexcept;
		RasterBatchBuilder&& SetIndexBuffer(re::VertexBufferInput const&)&& noexcept;

		RasterBatchBuilder&& SetDrawstyleBitmask(effect::drawstyle::Bitmask drawstyleBitmask) && noexcept;

		RasterBatchBuilder&& SetMaterialUniqueID(UniqueID)&& noexcept;


	private:
		RasterBatchBuilder(gr::RenderDataID) noexcept; // Instanced raster batches: Use create
		RasterBatchBuilder(re::Batch const& existingBatch) noexcept;
	};


	// ---


	class ComputeBatchBuilder : public IBatchBuilder<ComputeBatchBuilder>
	{
	public:
		ComputeBatchBuilder() noexcept : IBatchBuilder(re::Batch::BatchType::Compute) {}


	public:
		ComputeBatchBuilder&& SetThreadGroupCount(glm::uvec3&& threadGroupCount)&& noexcept;
		ComputeBatchBuilder&& SetThreadGroupCount(glm::uvec3 const& threadGroupCount)&& noexcept;
	};


	// ---


	class RayTraceBatchBuilder : public IBatchBuilder<RayTraceBatchBuilder>
	{
	public:
		RayTraceBatchBuilder() noexcept : IBatchBuilder(re::Batch::BatchType::RayTracing) {}


	public:
		RayTraceBatchBuilder&& SetOperation(re::Batch::RayTracingParams::Operation)&& noexcept;
		RayTraceBatchBuilder&& SetASInput(re::ASInput&&)&& noexcept;
		RayTraceBatchBuilder&& SetASInput(re::ASInput const&)&& noexcept;
		RayTraceBatchBuilder&& SetDispatchDimensions(glm::uvec3&& dispatchDimensions)&& noexcept;
		RayTraceBatchBuilder&& SetDispatchDimensions(glm::uvec3 const& dispatchDimensions)&& noexcept;
		RayTraceBatchBuilder&& SetRayGenShaderIdx(uint32_t rayGenShaderIdx)&& noexcept;
	};


	// ---


	template<typename BuilderImpl>
	IBatchBuilder<BuilderImpl>::IBatchBuilder(re::Batch::BatchType batchType) noexcept
		: m_batch(batchType)
		, m_renderDataID(gr::k_invalidRenderDataID)
	{
	}


	template<typename BuilderImpl>
	IBatchBuilder<BuilderImpl>::IBatchBuilder(re::Batch const& existingBatch) noexcept
		: m_batch(existingBatch)
		, m_renderDataID(gr::k_invalidRenderDataID)
	{
		SEAssert(existingBatch.GetType() != re::Batch::BatchType::Invalid, "Existing batch must not be invalid");
		m_batch.ResetDataHash(); // We're cloning the batch, reset the hash as we expect it will be modified
	}


	template<typename BuilderImpl>
	BuilderImpl&& IBatchBuilder<BuilderImpl>::SetEffectID(EffectID effectID) && noexcept
	{
		m_batch.SetEffectID(effectID);
		return static_cast<BuilderImpl&&>(*this);
	}


	template<typename BuilderImpl>
	BuilderImpl&& IBatchBuilder<BuilderImpl>::SetBuffer(
		std::string_view shaderName, std::shared_ptr<re::Buffer> const& buffer) && noexcept
	{
		m_batch.SetBuffer(shaderName.data(), buffer);
		return static_cast<BuilderImpl&&>(*this);
	}


	template<typename BuilderImpl>
	BuilderImpl&& IBatchBuilder<BuilderImpl>::SetBuffer(
		std::string_view shaderName, std::shared_ptr<re::Buffer> const& buffer, re::BufferView const& view) && noexcept
	{
		m_batch.SetBuffer(shaderName.data(), buffer, view);
		return static_cast<BuilderImpl&&>(*this);
	}


	template<typename BuilderImpl>
	BuilderImpl&& IBatchBuilder<BuilderImpl>::SetBuffer(re::BufferInput&& bufferInput) && noexcept
	{
		m_batch.SetBuffer(std::move(bufferInput));
		return static_cast<BuilderImpl&&>(*this);
	}


	template<typename BuilderImpl>
	BuilderImpl&& IBatchBuilder<BuilderImpl>::SetBuffer(re::BufferInput const& bufferInput) && noexcept
	{
		m_batch.SetBuffer(bufferInput);
		return static_cast<BuilderImpl&&>(*this);
	}


	template<typename BuilderImpl>
	BuilderImpl&& IBatchBuilder<BuilderImpl>::SetTextureInput(
		std::string_view shaderName,
		core::InvPtr<re::Texture> const& texture,
		core::InvPtr<re::Sampler> const& sampler,
		re::TextureView const& view)&& noexcept
	{
		m_batch.SetTextureInput(shaderName, texture, sampler, view);
		return static_cast<BuilderImpl&&>(*this);
	}


	template<typename BuilderImpl>
	BuilderImpl&& IBatchBuilder<BuilderImpl>::SetRWTextureInput(
		std::string_view shaderName,
		core::InvPtr<re::Texture> const& texture,
		re::TextureView const& view) && noexcept
	{
		m_batch.SetRWTextureInput(shaderName, texture, view);
		return static_cast<BuilderImpl&&>(*this);
	}


	template<typename BuilderImpl>
	BuilderImpl&& IBatchBuilder<BuilderImpl>::SetRootConstant(
		std::string_view shaderName, void const* src, re::DataType dataType) && noexcept
	{
		m_batch.SetRootConstant(shaderName, src, dataType);
		return static_cast<BuilderImpl&&>(*this);
	}


	template<typename BuilderImpl>
	BuilderImpl&& IBatchBuilder<BuilderImpl>::SetFilterMaskBit(re::Batch::Filter filterBit, bool enabled) && noexcept
	{
		m_batch.SetFilterMaskBit(filterBit, enabled);
		return static_cast<BuilderImpl&&>(*this);
	}


	template<typename BuilderImpl>
	gr::BatchHandle IBatchBuilder<BuilderImpl>::Build() && noexcept
	{
		m_batch.ComputeDataHash();
	
		return s_batchPool->AddBatch(std::move(m_batch), m_renderDataID);
	}


	// ---


	inline RasterBatchBuilder RasterBatchBuilder::CreateInstance(
		gr::RenderDataID renderDataID,
		gr::RenderDataManager const& renderDataMgr,
		BuildFromRenderDataCallback buildBatchCallback,
		re::Batch::VertexStreamOverride const* vertexStreamOverrides /*= nullptr*/) noexcept
	{
		return buildBatchCallback(
			RasterBatchBuilder(renderDataID), vertexStreamOverrides, renderDataID, renderDataMgr);
	}


	inline RasterBatchBuilder RasterBatchBuilder::CreateMeshPrimitiveBatch(
		core::InvPtr<gr::MeshPrimitive> const& meshPrim, EffectID effectID, BuildFromMeshPrimitiveCallback buildBatchCallback)
	{
		return buildBatchCallback(RasterBatchBuilder(), meshPrim, effectID);
	}


	inline RasterBatchBuilder RasterBatchBuilder::CloneAndModify(BatchHandle existingBatchHandle) noexcept
	{
		re::Batch const* existingBatch = s_batchPool->GetBatch(existingBatchHandle.GetPoolIndex());
		SEAssert(existingBatch != nullptr, "Existing batch must not be null");
		SEAssert(existingBatch->GetType() == re::Batch::BatchType::Raster, "Existing batch must be a raster batch");

		return RasterBatchBuilder(*existingBatch);
	}


	inline RasterBatchBuilder::RasterBatchBuilder(gr::RenderDataID renderDataID) noexcept
		: IBatchBuilder<RasterBatchBuilder>(re::Batch::BatchType::Raster)
	{
		m_renderDataID = renderDataID;
	}


	inline RasterBatchBuilder::RasterBatchBuilder(re::Batch const& existingBatch) noexcept
		: IBatchBuilder(existingBatch)
	{
		SEAssert(existingBatch.GetType() == re::Batch::BatchType::Raster, "Existing batch must be a raster batch");
	}


	inline RasterBatchBuilder&& RasterBatchBuilder::SetGeometryMode(re::Batch::GeometryMode geoMode) && noexcept
	{
		SEAssert(geoMode != re::Batch::GeometryMode::Invalid, "Invalid geometry mode");
		m_batch.m_rasterParams.m_batchGeometryMode = geoMode;
		return std::move(*this);
	}


	inline RasterBatchBuilder&& RasterBatchBuilder::SetPrimitiveTopology(
		gr::MeshPrimitive::PrimitiveTopology primitiveTopology) && noexcept
	{
		m_batch.m_rasterParams.m_primitiveTopology = primitiveTopology;
		return std::move(*this);
	}


	inline RasterBatchBuilder&& RasterBatchBuilder::SetVertexBuffer(
		uint8_t slotIdx, re::VertexBufferInput&& vertexBufferInput) && noexcept
	{
		SEAssert(slotIdx < re::VertexStream::k_maxVertexStreams, "Invalid vertex stream slot index");
		m_batch.m_rasterParams.m_vertexBuffers[slotIdx] = std::move(vertexBufferInput);
		return std::move(*this);
	}


	inline RasterBatchBuilder&& RasterBatchBuilder::SetVertexBuffer(
		uint8_t slotIdx, re::VertexBufferInput const& vertexBufferInput) && noexcept
	{
		SEAssert(slotIdx < re::VertexStream::k_maxVertexStreams, "Invalid vertex stream slot index");
		m_batch.m_rasterParams.m_vertexBuffers[slotIdx] = vertexBufferInput;
		return std::move(*this);
	}


	inline RasterBatchBuilder&& RasterBatchBuilder::SetVertexBuffers(
		std::array<re::VertexBufferInput, re::VertexStream::k_maxVertexStreams>&& vertexBuffers) && noexcept
	{
		m_batch.m_rasterParams.m_vertexBuffers = std::move(vertexBuffers);
		return std::move(*this);
	}


	inline RasterBatchBuilder&& RasterBatchBuilder::SetVertexBuffers(
		std::array<re::VertexBufferInput, re::VertexStream::k_maxVertexStreams> const& vertexBuffers) && noexcept
	{
		m_batch.m_rasterParams.m_vertexBuffers = vertexBuffers;
		return std::move(*this);
	}


	inline RasterBatchBuilder&& RasterBatchBuilder::SetVertexStreamOverrides(
		re::Batch::VertexStreamOverride const* vertexStreamOverrides) && noexcept
	{
		SEAssert(vertexStreamOverrides, "vertexStreamOverrides is null");

		m_batch.m_rasterParams.m_vertexStreamOverrides = vertexStreamOverrides;
		return std::move(*this);
	}


	inline RasterBatchBuilder&& RasterBatchBuilder::SetIndexBuffer(re::VertexBufferInput&& indexBufferInput) && noexcept
	{
		m_batch.m_rasterParams.m_indexBuffer = std::move(indexBufferInput);
		return std::move(*this);
	}


	inline RasterBatchBuilder&& RasterBatchBuilder::SetIndexBuffer(re::VertexBufferInput const& indexBufferInput) && noexcept
	{
		m_batch.m_rasterParams.m_indexBuffer = indexBufferInput;
		return std::move(*this);
	}


	inline RasterBatchBuilder&& RasterBatchBuilder::SetDrawstyleBitmask(effect::drawstyle::Bitmask drawstyleBitmask) && noexcept
	{
		m_batch.SetDrawstyleBits(drawstyleBitmask);
		return std::move(*this);
	}


	inline RasterBatchBuilder&& RasterBatchBuilder::SetMaterialUniqueID(UniqueID materialID) && noexcept
	{
		m_batch.m_rasterParams.m_materialUniqueID = materialID;
		return std::move(*this);
	}


	// ---


	inline ComputeBatchBuilder&& ComputeBatchBuilder::SetThreadGroupCount(glm::uvec3&& threadGroupCount) && noexcept
	{
		m_batch.m_computeParams.m_threadGroupCount = std::move(threadGroupCount);
		return std::move(*this);
	}


	inline ComputeBatchBuilder&& ComputeBatchBuilder::SetThreadGroupCount(glm::uvec3 const& threadGroupCount) && noexcept
	{
		m_batch.m_computeParams.m_threadGroupCount = threadGroupCount;
		return std::move(*this);
	}


	// ---


	inline RayTraceBatchBuilder&& RayTraceBatchBuilder::SetOperation(re::Batch::RayTracingParams::Operation operation) && noexcept
	{
		m_batch.m_rayTracingParams.m_operation = operation;
		return std::move(*this);
	}


	inline RayTraceBatchBuilder&& RayTraceBatchBuilder::SetASInput(re::ASInput&& asInput) && noexcept
	{
		m_batch.m_rayTracingParams.m_ASInput = std::move(asInput);
		return std::move(*this);
	}


	inline RayTraceBatchBuilder&& RayTraceBatchBuilder::SetASInput(re::ASInput const& asInput) && noexcept
	{
		m_batch.m_rayTracingParams.m_ASInput = asInput;
		return std::move(*this);
	}


	inline RayTraceBatchBuilder&& RayTraceBatchBuilder::SetDispatchDimensions(glm::uvec3&& dispatchDimensions) && noexcept
	{
		m_batch.m_rayTracingParams.m_dispatchDimensions = std::move(dispatchDimensions);
		return std::move(*this);
	}


	inline RayTraceBatchBuilder&& RayTraceBatchBuilder::SetDispatchDimensions(glm::uvec3 const& dispatchDimensions) && noexcept
	{
		m_batch.m_rayTracingParams.m_dispatchDimensions = dispatchDimensions;
		return std::move(*this);
	}


	inline RayTraceBatchBuilder&& RayTraceBatchBuilder::SetRayGenShaderIdx(uint32_t rayGenShaderIdx) && noexcept
	{
		m_batch.m_rayTracingParams.m_rayGenShaderIdx = rayGenShaderIdx;
		return std::move(*this);
	}
}