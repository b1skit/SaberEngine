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

namespace re
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

		enum class GeometryMode : uint8_t
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
			friend class re::Batch;
			friend class gr::RasterBatchBuilder;
			friend class gr::StageBatchHandle;

		protected: // Use the StageBatchHandle's resolved vertex buffers instead
			// Vertex streams must be contiguously packed, with streams of the same type stored consecutively
			std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams> m_vertexBuffers{};

			VertexBufferInput m_indexBuffer{};


		public:
			// If a batch is created via a RenderDataID associated with a gr::Material::MaterialInstanceRenderData, we 
			// store the material's unique ID so we can include it in the data hash to ensure batches with identical
			// geometry and materials will sort together
			UniqueID m_materialUniqueID = k_invalidUniqueID;

			GeometryMode m_batchGeometryMode = GeometryMode::Invalid;
			gr::MeshPrimitive::PrimitiveTopology m_primitiveTopology = gr::MeshPrimitive::PrimitiveTopology::TriangleList;
		};
		using VertexStreamOverride = std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams>;

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

		std::vector<BufferInput> const& GetBuffers() const;
		std::vector<TextureAndSamplerInput> const& GetTextureAndSamplerInputs() const;
		std::vector<RWTextureInput> const& GetRWTextureInputs() const;
		RootConstants const& GetRootConstants() const;

		FilterBitmask GetBatchFilterMask() const;
		bool MatchesFilterBits(re::Batch::FilterBitmask required, re::Batch::FilterBitmask excluded) const;

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
		void SetFilterMaskBit(re::Batch::Filter filterBit, bool enabled);


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
		std::vector<BufferInput> m_batchBuffers;

		std::vector<TextureAndSamplerInput> m_batchTextureSamplerInputs;
		std::vector<RWTextureInput> m_batchRWTextureInputs;

		RootConstants m_batchRootConstants;


	private:
		friend class gr::BatchPoolPage;
		friend class gr::ComputeBatchBuilder;
		template<typename BuilderImpl>
		friend class gr::IBatchBuilder;
		friend class gr::RasterBatchBuilder;
		friend class gr::RayTraceBatchBuilder;
	};


	// ---


	inline re::Batch::BatchType Batch::GetType() const
	{
		return m_type;
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


	inline re::Batch::FilterBitmask Batch::GetBatchFilterMask() const
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