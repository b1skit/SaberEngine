// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "AccelerationStructure.h"
#include "BufferView.h"
#include "Effect.h"
#include "EnumTypes.h"
#include "MeshPrimitive.h"
#include "RootConstants.h"
#include "Sampler.h"
#include "TextureView.h"
#include "VertexStream.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/IHashedDataObject.h"
#include "Core/Interfaces/IUniqueID.h"


namespace gr
{
	class ComputeBatchBuilder;
	template<typename BuilderImpl>
	class IBatchBuilder;
	class RasterBatchBuilder;
	class RayTraceBatchBuilder;
}

namespace re
{
	class BatchHandle;
	class Buffer;
	class Shader;
	class Texture;
}

namespace re
{
	class Batch final : public virtual core::IHashedDataObject
	{
	public:
		enum class BatchType
		{
			Raster,
			Compute,
			RayTracing,

			Invalid,
		};

		enum class GeometryMode
		{
			// Note: All draws are instanced, even if an API supports non-instanced drawing
			IndexedInstanced,
			ArrayInstanced,

			Invalid
		};

		using FilterBitmask = uint32_t;
		enum Filter : FilterBitmask
		{
			AlphaBlended	= 1 << 0,
			ShadowCaster	= 1 << 1,

			Filter_Count
		};
		SEStaticAssert(re::Batch::Filter::Filter_Count <= 32, "Too many filter bits");

		struct RasterParams final
		{		
			GeometryMode m_batchGeometryMode = GeometryMode::Invalid;
			uint32_t m_numInstances = 0;
			gr::MeshPrimitive::PrimitiveTopology m_primitiveTopology = gr::MeshPrimitive::PrimitiveTopology::TriangleList;

			// Vertex streams must be contiguously packed, with streams of the same type stored consecutively
			std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams> m_vertexBuffers{};

			VertexBufferInput m_indexBuffer{};

			// If a batch is created via the CTOR that takes a gr::Material::MaterialInstanceRenderData, we store the 
			// material's unique ID so we can include it in the data hash to ensure batches with identical geometry and
			// materials will sort together
			UniqueID m_materialUniqueID = k_invalidUniqueID;
		};
		
		struct ComputeParams final
		{
			// No. groups dispatched in XYZ directions:
			glm::uvec3 m_threadGroupCount = glm::uvec3(0); 
		};
		
		struct RayTracingParams final
		{
			enum class Operation : uint8_t
			{
				BuildAS,
				UpdateAS,
				CompactAS,

				DispatchRays,

				Invalid
			} m_operation = Operation::Invalid;

			re::ASInput m_ASInput; // BLAS or TLAS, depending on the operation

			glm::uvec3 m_dispatchDimensions = glm::uvec3(0); // .xyz = DispatchRays() width/height/depth
			uint32_t m_rayGenShaderIdx = 0;
		};
		using VertexStreamOverride = std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams>;


	private:
		friend class BatchHandle;

		Batch(BatchType batchType); // Used by BatchBuilders

		Batch(Batch&&) noexcept;
		Batch(Batch const&) noexcept;

		Batch& operator=(Batch&&) noexcept;
		Batch& operator=(Batch const&) noexcept;

		void Destroy();
		

	public:
		~Batch();


	public:		
		static Batch Duplicate(Batch const&, re::Lifetime);


	public:
		BatchType GetType() const;

		size_t GetInstanceCount() const;

		core::InvPtr<re::Shader> const& GetShader() const;
		EffectID GetEffectID() const;

		std::vector<BufferInput> const& GetBuffers() const;
		std::vector<TextureAndSamplerInput> const& GetTextureAndSamplerInputs() const;
		std::vector<RWTextureInput> const& GetRWTextureInputs() const;
		RootConstants const& GetRootConstants() const;

		re::Lifetime GetLifetime() const;

		FilterBitmask GetBatchFilterMask() const;

		RasterParams const& GetRasterParams() const;
		ComputeParams const& GetComputeParams() const;
		RayTracingParams const& GetRayTracingParams() const;


	private:
		void SetRenderDataID(gr::RenderDataID);
		void SetEffectID(EffectID);
		void SetDrawstyleBits(effect::drawstyle::Bitmask);
		void SetLifetime(re::Lifetime);
	

	public: // TODO: Make these private
		void SetInstanceCount(uint32_t numInstances);

		void SetBuffer(std::string_view shaderName, std::shared_ptr<re::Buffer> const&);
		void SetBuffer(std::string_view shaderName, std::shared_ptr<re::Buffer> const&, re::BufferView const&);
		void SetBuffer(re::BufferInput&&);
		void SetBuffer(re::BufferInput const&);
	
		void SetTextureInput(
			std::string_view shaderName,
			core::InvPtr<re::Texture> const&,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);
	

	private:
		void SetRWTextureInput(
			std::string_view shaderName,
			core::InvPtr<re::Texture> const&,
			re::TextureView const&);

		void SetRootConstant(std::string_view shaderName, void const* src, re::DataType);
		void SetFilterMaskBit(re::Batch::Filter filterBit, bool enabled);


	public: // TODO: Make these private
		bool MatchesFilterBits(re::Batch::FilterBitmask required, re::Batch::FilterBitmask excluded) const;

		void Finalize(effect::drawstyle::Bitmask stageBitmask); // Called by the owning re::Stage


	private:
		void ComputeBatchHash();


	private: 
		void ComputeDataHash() override; // core::IHashedDataObject


	private:
		re::Lifetime m_lifetime;
		BatchType m_type;

		union
		{
			RasterParams m_rasterParams;
			ComputeParams m_computeParams;
			RayTracingParams m_rayTracingParams;
		};
		friend class gr::ComputeBatchBuilder;
		template<typename BuilderImpl>
		friend class gr::IBatchBuilder;
		friend class gr::RasterBatchBuilder;
		friend class gr::RayTraceBatchBuilder;
		
		core::InvPtr<re::Shader> m_batchShader;

		EffectID m_effectID;
		effect::drawstyle::Bitmask m_drawStyleBitmask;
		FilterBitmask m_batchFilterBitmask;

		// Render data ID of the batch, if created from render data
		gr::RenderDataID m_renderDataID; 


	private:
		static constexpr size_t k_batchBufferIDsReserveAmount = 8;
		static constexpr size_t k_texSamplerInputReserveAmount = 8;
		static constexpr size_t k_rwTextureInputReserveAmount = 8;

		// Note: Batches can be responsible for the lifetime of a buffer held by a shared pointer: 
		// e.g. single-frame resources, or permanent buffers that are to be discarded (e.g. batch manager allocated a larger
		// one)
		std::vector<BufferInput> m_batchBuffers;

		std::vector<TextureAndSamplerInput> m_batchTextureSamplerInputs;
		std::vector<RWTextureInput> m_batchRWTextureInputs;

		RootConstants m_batchRootConstants;


	private:
		Batch() = delete;
	};


	// ---


	class BatchHandle final
	{
		// TODO: Eventually this will be a lightweight wrapper around an integer handle to Batches held in a pool
	public:
		BatchHandle(re::Batch&& batch) noexcept
			: m_batch(std::move(batch))
		{
		}

		BatchHandle(re::Batch const& batch) noexcept
			: BatchHandle(re::Batch(batch))
		{
		}

		BatchHandle(BatchHandle&&) noexcept = default;
		BatchHandle& operator=(BatchHandle&&) noexcept = default;
		BatchHandle(BatchHandle const&) noexcept = default;
		BatchHandle& operator=(BatchHandle const&) noexcept = default;

		~BatchHandle() noexcept = default;


		re::Batch* operator->() noexcept
		{
			return &m_batch;
		}

		re::Batch const* operator->() const noexcept
		{
			return &m_batch;
		}

		re::Batch& operator*() noexcept
		{
			return m_batch;
		}

		re::Batch const& operator*() const noexcept
		{
			return m_batch;
		}


	private:
		friend class gr::ComputeBatchBuilder;
		template<typename BuilderImpl>
		friend class gr::IBatchBuilder;
		friend class gr::RasterBatchBuilder;
		friend class gr::RayTraceBatchBuilder;

		re::Batch m_batch;
	};


	// ---


	inline re::Batch::BatchType Batch::GetType() const
	{
		return m_type;
	}


	inline void Batch::SetRenderDataID(gr::RenderDataID renderDataID)
	{
		SEAssert(m_renderDataID == gr::k_invalidRenderDataID, "RenderDataID already set. This is unexpected");
		m_renderDataID = renderDataID;
	}


	inline void Batch::SetEffectID(EffectID effectID)
	{
		SEAssert(m_effectID == 0, "EffectID has already been set. This is unexpected");
		m_effectID = effectID;
	}


	inline EffectID Batch::GetEffectID() const
	{
		return m_effectID;
	}


	inline void Batch::SetDrawstyleBits(effect::drawstyle::Bitmask drawstyleBits)
	{
		SEAssert(m_drawStyleBitmask == 0, "Drawstyle bits already set, this is unexpected");
		m_drawStyleBitmask = drawstyleBits;
	}


	inline void Batch::SetLifetime(re::Lifetime lifetime)
	{
		m_lifetime = lifetime;
	}


	inline void Batch::ComputeBatchHash()
	{
		ComputeDataHash();
	}


	inline core::InvPtr<re::Shader> const& Batch::GetShader() const
	{
		return m_batchShader;
	}


	inline size_t Batch::GetInstanceCount() const
	{
		SEAssert(m_type == BatchType::Raster, "Invalid type");
		return m_rasterParams.m_numInstances;
	}


	inline std::vector<BufferInput> const& Batch::GetBuffers() const
	{
		return m_batchBuffers;
	}


	inline std::vector<re::TextureAndSamplerInput> const& Batch::GetTextureAndSamplerInputs() const
	{
		return m_batchTextureSamplerInputs;
	}


	inline std::vector<RWTextureInput> const& Batch::GetRWTextureInputs() const
	{
		return m_batchRWTextureInputs;
	}


	inline void Batch::SetRootConstant(std::string_view shaderName, void const* src, re::DataType dataType)
	{
		m_batchRootConstants.SetRootConstant(shaderName, src, dataType);
	}


	inline RootConstants const& Batch::GetRootConstants() const
	{
		return m_batchRootConstants;
	}


	inline re::Lifetime Batch::GetLifetime() const
	{
		return m_lifetime;
	}


	inline re::Batch::FilterBitmask Batch::GetBatchFilterMask() const
	{
		return m_batchFilterBitmask;
	}


	inline Batch::RasterParams const& Batch::GetRasterParams() const
	{
		SEAssert(m_type == BatchType::Raster, "Invalid type");
		return m_rasterParams;
	}


	inline Batch::ComputeParams const& Batch::GetComputeParams() const
	{
		SEAssert(m_type == BatchType::Compute, "Invalid type");
		return m_computeParams;
	}


	inline Batch::RayTracingParams const& Batch::GetRayTracingParams() const
	{
		SEAssert(m_type == BatchType::RayTracing, "Invalid type");
		return m_rayTracingParams;
	}
}