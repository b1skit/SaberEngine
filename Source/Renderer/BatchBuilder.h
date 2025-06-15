// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Batch.h"
#include "BufferView.h"
#include "EnumTypes.h"
#include "RenderObjectIDs.h"
#include "MeshPrimitive.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/IUniqueID.h"


namespace re
{
	class Buffer;
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
	public:
		IBatchBuilder(re::Batch::BatchType) noexcept;
		
		IBatchBuilder(IBatchBuilder&&) noexcept = default;
		IBatchBuilder& operator=(IBatchBuilder&&) noexcept = default;
		virtual ~IBatchBuilder() noexcept = default;

		inline IBatchBuilder&& operator()() noexcept { return std::move(*this); }; // Convenience accessor


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
		re::BatchHandle Build(re::Lifetime) && noexcept;
		re::BatchHandle BuildPermanent()&& noexcept;
		re::BatchHandle BuildSingleFrame()&& noexcept;
		

	protected:
		re::Batch m_batch; // The batch we're building


	private: // R-value only:
		IBatchBuilder(IBatchBuilder const&) = delete;
		IBatchBuilder operator=(IBatchBuilder const&) = delete;
		IBatchBuilder() noexcept = delete;
	};


	// ---


	class RasterBatchBuilder : public IBatchBuilder<RasterBatchBuilder>
	{
	public:
		RasterBatchBuilder() noexcept : IBatchBuilder(re::Batch::BatchType::Raster) {}
		
		inline RasterBatchBuilder&& operator()() noexcept { return std::move(*this); }; // Convenience accessor


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


	public:
		RasterBatchBuilder&& SetGeometryMode(re::Batch::GeometryMode)&& noexcept;
		RasterBatchBuilder&& SetPrimitiveTopology(gr::MeshPrimitive::PrimitiveTopology)&& noexcept;

		RasterBatchBuilder&& SetVertexBuffer(uint8_t slotIdx, re::VertexBufferInput&&)&& noexcept;
		RasterBatchBuilder&& SetVertexBuffer(uint8_t slotIdx, re::VertexBufferInput const&)&& noexcept;

		RasterBatchBuilder&& SetVertexBuffers(std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams>&&)&& noexcept;
		RasterBatchBuilder&& SetVertexBuffers(std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams>const&)&& noexcept;

		RasterBatchBuilder&& SetVertexStreamOverrides(re::Batch::VertexStreamOverride const*)&& noexcept;

		RasterBatchBuilder&& SetIndexBuffer(re::VertexBufferInput&&)&& noexcept;
		RasterBatchBuilder&& SetIndexBuffer(re::VertexBufferInput const&)&& noexcept;

		RasterBatchBuilder&& SetDrawstyleBitmask(effect::drawstyle::Bitmask drawstyleBitmask) && noexcept;

		RasterBatchBuilder&& SetMaterialUniqueID(UniqueID)&& noexcept;

		// TODO: Replace this once we're using BatchHandles and resolving instancing as a post-processing step
		RasterBatchBuilder&& SetNumInstances_TEMP(uint32_t numInstances) && noexcept;


	private:
		RasterBatchBuilder(gr::RenderDataID) noexcept; // Instanced raster batches: Use create
	};


	// ---


	class ComputeBatchBuilder : public IBatchBuilder<ComputeBatchBuilder>
	{
	public:
		ComputeBatchBuilder() noexcept : IBatchBuilder(re::Batch::BatchType::Compute) {}

		inline ComputeBatchBuilder&& operator()() noexcept { return std::move(*this); }; // Convenience accessor


	public:
		ComputeBatchBuilder&& SetThreadGroupCount(glm::uvec3&& threadGroupCount)&& noexcept;
		ComputeBatchBuilder&& SetThreadGroupCount(glm::uvec3 const& threadGroupCount)&& noexcept;
	};


	// ---


	class RayTraceBatchBuilder : public IBatchBuilder<RayTraceBatchBuilder>
	{
	public:
		RayTraceBatchBuilder() noexcept : IBatchBuilder(re::Batch::BatchType::RayTracing) {}

		inline RayTraceBatchBuilder&& operator()() noexcept { return std::move(*this); }; // Convenience accessor


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
	{
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
		re::TextureView const& view) && noexcept
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
	re::BatchHandle IBatchBuilder<BuilderImpl>::Build(re::Lifetime lifetime) && noexcept
	{
		m_batch.SetLifetime(lifetime); // TODO: Use this to move the batch to an appropriate Batch pool
		m_batch.ComputeBatchHash();

		return re::BatchHandle(std::move(m_batch));
	}


	template<typename BuilderImpl>
	re::BatchHandle IBatchBuilder<BuilderImpl>::BuildPermanent() && noexcept
	{
		return std::move(static_cast<BuilderImpl&&>(*this)).Build(re::Lifetime::Permanent);
	}


	template<typename BuilderImpl>
	re::BatchHandle IBatchBuilder<BuilderImpl>::BuildSingleFrame() && noexcept
	{
		return std::move(static_cast<BuilderImpl&&>(*this)).Build(re::Lifetime::SingleFrame);
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


	inline RasterBatchBuilder::RasterBatchBuilder(gr::RenderDataID renderDataID) noexcept
		: IBatchBuilder<RasterBatchBuilder>(re::Batch::BatchType::Raster)
	{
		m_batch.SetRenderDataID(renderDataID);
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
		SEAssert(slotIdx < gr::VertexStream::k_maxVertexStreams, "Invalid vertex stream slot index");
		m_batch.m_rasterParams.m_vertexBuffers[slotIdx] = std::move(vertexBufferInput);
		return std::move(*this);
	}


	inline RasterBatchBuilder&& RasterBatchBuilder::SetVertexBuffer(
		uint8_t slotIdx, re::VertexBufferInput const& vertexBufferInput) && noexcept
	{
		SEAssert(slotIdx < gr::VertexStream::k_maxVertexStreams, "Invalid vertex stream slot index");
		m_batch.m_rasterParams.m_vertexBuffers[slotIdx] = vertexBufferInput;
		return std::move(*this);
	}


	inline RasterBatchBuilder&& RasterBatchBuilder::SetVertexBuffers(
		std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams>&& vertexBuffers) && noexcept
	{
		m_batch.m_rasterParams.m_vertexBuffers = std::move(vertexBuffers);
		return std::move(*this);
	}


	inline RasterBatchBuilder&& RasterBatchBuilder::SetVertexBuffers(
		std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams> const& vertexBuffers) && noexcept
	{
		m_batch.m_rasterParams.m_vertexBuffers = vertexBuffers;
		return std::move(*this);
	}


	inline RasterBatchBuilder&& RasterBatchBuilder::SetVertexStreamOverrides(
		re::Batch::VertexStreamOverride const* vertexStreamOverrides) && noexcept
	{
		SEAssert(vertexStreamOverrides, "vertexStreamOverrides is null");

		m_batch.m_rasterParams.m_vertexBuffers = *vertexStreamOverrides;
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


	inline RasterBatchBuilder&& RasterBatchBuilder::SetNumInstances_TEMP(uint32_t numInstances) && noexcept
	{
		m_batch.m_rasterParams.m_numInstances = numInstances;
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