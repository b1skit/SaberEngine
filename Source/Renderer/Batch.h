// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "AccelerationStructure.h"
#include "BufferView.h"
#include "Effect.h"
#include "EnumTypes.h"
#include "RasterState.h"
#include "RootConstants.h"
#include "Sampler.h"
#include "TextureView.h"
#include "VertexStream.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/IHashedDataObject.h"
#include "Core/Interfaces/IUniqueID.h"

#include "_generated/DrawStyles.h"


namespace gr
{
	class BatchPoolPage;
	class ComputeBatchBuilder;
	template<typename BuilderImpl>
	class IBatchBuilder;
	class RasterBatchBuilder;
	class RayTraceBatchBuilder;
	class StageBatchHandle;
}

namespace re
{
	class Buffer;
	class Shader;
	class Texture;
}

namespace gr
{
	class Batch final : public virtual core::IHashedDataObject
	{
	public:
		enum class BatchType : uint8_t
		{
			Raster,
			Compute,
			RayTracing,

			Invalid,
		};

		using FilterBitmask = uint32_t;
		enum Filter : FilterBitmask
		{
			AlphaBlended	= 1 << 0,
			ShadowCaster	= 1 << 1,

			Filter_Count
		};
		SEStaticAssert(gr::Batch::Filter::Filter_Count <= 32, "Too many filter bits");

		using VertexStreamOverride = std::array<re::VertexBufferInput, re::VertexStream::k_maxVertexStreams>;
		struct RasterParams final
		{
		private:
			friend class gr::Batch;
			friend class gr::RasterBatchBuilder;
			friend class gr::StageBatchHandle;

		protected: // Use the StageBatchHandle's resolved vertex buffers instead
			// Vertex streams must be contiguously packed, with streams of the same type stored consecutively
			std::array<re::VertexBufferInput, re::VertexStream::k_maxVertexStreams> m_vertexBuffers{};

			re::VertexBufferInput m_indexBuffer{};

		protected:
			// Optional overrides for vertex streams (e.g. animation)
			gr::Batch::VertexStreamOverride const* m_vertexStreamOverrides = nullptr;


		public:
			bool HasVertexStream(re::VertexStream::Type) const noexcept;
			re::VertexBufferInput const* GetVertexStreamInput(re::VertexStream::Type, uint8_t setIdx = 0) const noexcept;


		public:
			// If a batch is created via a RenderDataID associated with a gr::Material::MaterialInstanceRenderData, we 
			// store the material's unique ID so we can include it in the data hash to ensure batches with identical
			// geometry and materials will sort together
			UniqueID m_materialUniqueID = k_invalidUniqueID;

			re::GeometryMode m_batchGeometryMode = re::GeometryMode::Invalid;

			re::RasterState::PrimitiveTopology m_primitiveTopology = re::RasterState::PrimitiveTopology::TriangleList;
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


	private:
		Batch(); // Invalid/uninitialized
		
		Batch(BatchType batchType); // Used by BatchBuilders

		Batch(Batch&&) noexcept;
		Batch& operator=(Batch&&) noexcept;

		Batch(Batch const&) noexcept;
		Batch& operator=(Batch const&) noexcept;
		
		void Destroy();


	public:
		~Batch();


	public:
		BatchType GetType() const;

		EffectID GetEffectID() const;

		std::vector<re::BufferInput> const& GetBuffers() const;
		std::vector<re::TextureAndSamplerInput> const& GetTextureAndSamplerInputs() const;
		std::vector<re::RWTextureInput> const& GetRWTextureInputs() const;
		re::RootConstants const& GetRootConstants() const;

		FilterBitmask GetBatchFilterMask() const;
		bool MatchesFilterBits(gr::Batch::FilterBitmask required, gr::Batch::FilterBitmask excluded) const;

		effect::drawstyle::Bitmask GetDrawstyleBits() const;

		RasterParams const& GetRasterParams() const;
		ComputeParams const& GetComputeParams() const;
		RayTracingParams const& GetRayTracingParams() const;

		bool IsValid() const;


	private:
		void SetEffectID(EffectID);
		void SetDrawstyleBits(effect::drawstyle::Bitmask);

		void SetBuffer(std::string_view shaderName, std::shared_ptr<re::Buffer> const&);
		void SetBuffer(std::string_view shaderName, std::shared_ptr<re::Buffer> const&, re::BufferView const&);
		void SetBuffer(re::BufferInput&&);
		void SetBuffer(re::BufferInput const&);
	
		void SetTextureInput(
			std::string_view shaderName,
			core::InvPtr<re::Texture> const&,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);
	
		void SetRWTextureInput(
			std::string_view shaderName,
			core::InvPtr<re::Texture> const&,
			re::TextureView const&);

		void SetRootConstant(std::string_view shaderName, void const* src, re::DataType);
		void SetFilterMaskBit(gr::Batch::Filter filterBit, bool enabled);


	private:
		void ComputeDataHash() override; // core::IHashedDataObject


	private:
		BatchType m_type;

		union
		{
			RasterParams m_rasterParams;
			ComputeParams m_computeParams;
			RayTracingParams m_rayTracingParams;
		};

		EffectID m_effectID;
		effect::drawstyle::Bitmask m_drawStyleBitmask;
		FilterBitmask m_batchFilterBitmask;


	private:
		static constexpr size_t k_batchBufferIDsReserveAmount = 8;
		static constexpr size_t k_texSamplerInputReserveAmount = 8;
		static constexpr size_t k_rwTextureInputReserveAmount = 8;

		// Note: Batches can be responsible for the lifetime of a buffer held by a shared pointer: 
		// e.g. single-frame resources, or permanent buffers that are to be discarded (e.g. batch manager allocated a larger
		// one)
		std::vector<re::BufferInput> m_batchBuffers;

		std::vector<re::TextureAndSamplerInput> m_batchTextureSamplerInputs;
		std::vector<re::RWTextureInput> m_batchRWTextureInputs;

		re::RootConstants m_batchRootConstants;


	private:
		friend class gr::BatchPoolPage;
		friend class gr::ComputeBatchBuilder;
		template<typename BuilderImpl>
		friend class gr::IBatchBuilder;
		friend class gr::RasterBatchBuilder;
		friend class gr::RayTraceBatchBuilder;
	};


	// ---


	inline bool gr::Batch::RasterParams::HasVertexStream(re::VertexStream::Type streamType) const noexcept
	{
		for (auto const& vertexBuffer : m_vertexBuffers)
		{
			if (vertexBuffer.GetStream() == nullptr)
			{
				return false;
			}
			if (vertexBuffer.GetStream()->GetType() == streamType)
			{
				return true;
			}
		}
		return false;
	}


	inline re::VertexBufferInput const* gr::Batch::RasterParams::GetVertexStreamInput(
		re::VertexStream::Type streamType, uint8_t setIdx /*= 0*/) const noexcept
	{
		SEAssert(streamType != re::VertexStream::Type::Type_Count, "Invalid vertex stream type");

		if (streamType == re::VertexStream::Type::Index)
		{
			if (m_indexBuffer.GetStream() == nullptr)
			{
				return nullptr; // No index buffer set, return nullptr here to be consistent with behavior below
			}
			return &m_indexBuffer;
		}

		for (uint8_t streamIdx = 0; streamIdx < m_vertexBuffers.size(); ++streamIdx)
		{
			if (m_vertexBuffers[streamIdx].GetStream()->GetType() == re::VertexStream::Type_Count)
			{
				return nullptr;
			}

			if (m_vertexBuffers[streamIdx].GetStream()->GetType() == streamType)
			{
				const uint8_t offsetIdx = streamIdx + setIdx;
				SEAssert(offsetIdx < m_vertexBuffers.size(), "Invalid set index");

				// Note: It is valid to find a stream with a different type (e.g. UV1 doesn't exist)
				if (m_vertexBuffers[offsetIdx].GetStream()->GetType() == streamType)
				{
					return &m_vertexBuffers[offsetIdx];
				}
			}
		}
		return nullptr;
	}


	// ---


	inline gr::Batch::BatchType Batch::GetType() const
	{
		return m_type;
	}


	inline void Batch::SetEffectID(EffectID effectID)
	{
		m_effectID = effectID;
	}


	inline EffectID Batch::GetEffectID() const
	{
		return m_effectID;
	}


	inline void Batch::SetDrawstyleBits(effect::drawstyle::Bitmask drawstyleBits)
	{
		m_drawStyleBitmask = drawstyleBits;
	}


	inline std::vector<re::BufferInput> const& Batch::GetBuffers() const
	{
		return m_batchBuffers;
	}


	inline std::vector<re::TextureAndSamplerInput> const& Batch::GetTextureAndSamplerInputs() const
	{
		return m_batchTextureSamplerInputs;
	}


	inline std::vector<re::RWTextureInput> const& Batch::GetRWTextureInputs() const
	{
		return m_batchRWTextureInputs;
	}


	inline void Batch::SetRootConstant(std::string_view shaderName, void const* src, re::DataType dataType)
	{
		m_batchRootConstants.SetRootConstant(shaderName, src, dataType);
	}


	inline re::RootConstants const& Batch::GetRootConstants() const
	{
		return m_batchRootConstants;
	}


	inline gr::Batch::FilterBitmask Batch::GetBatchFilterMask() const
	{
		return m_batchFilterBitmask;
	}


	inline effect::drawstyle::Bitmask Batch::GetDrawstyleBits() const
	{
		return m_drawStyleBitmask;
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


	inline bool Batch::IsValid() const
	{
		return m_type != BatchType::Invalid;
	}
}