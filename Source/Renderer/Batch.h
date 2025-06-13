// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "AccelerationStructure.h"
#include "BufferView.h"
#include "Effect.h"
#include "EnumTypes.h"
#include "Material.h"
#include "MeshPrimitive.h"
#include "RootConstants.h"
#include "Sampler.h"
#include "TextureView.h"
#include "VertexStream.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/IHashedDataObject.h"
#include "Core/Interfaces/IUniqueID.h"


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
		enum class BatchType
		{
			Raster,
			Compute,
			RayTracing,
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
			RasterParams();
			RasterParams(RasterParams const&) noexcept;
			RasterParams(RasterParams&&) noexcept;
			RasterParams& operator=(RasterParams const&) noexcept;
			RasterParams& operator=(RasterParams&&) noexcept;
			~RasterParams();
			
			GeometryMode m_batchGeometryMode = GeometryMode::Invalid;
			uint32_t m_numInstances = 0;
			gr::MeshPrimitive::PrimitiveTopology m_primitiveTopology = gr::MeshPrimitive::PrimitiveTopology::TriangleList;

			// Vertex streams must be contiguously packed, with streams of the same type stored consecutively
			std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams> m_vertexBuffers{};

			VertexBufferInput m_indexBuffer{};

			// If a batch is created via the CTOR that takes a gr::Material::MaterialInstanceRenderData, we store the 
			// material's unique ID so we can include it in the data hash to ensure batches with identical geometry and
			// materials will sort together
			uint64_t m_materialUniqueID = core::IUniqueID::k_invalidUniqueID;
		};
		struct ComputeParams final
		{
			// No. groups dispatched in XYZ directions:
			glm::uvec3 m_threadGroupCount = glm::uvec3(0); 
		};
		struct RayTracingParams final
		{
			RayTracingParams();
			RayTracingParams(RayTracingParams const&) noexcept;
			RayTracingParams(RayTracingParams&&) noexcept;
			RayTracingParams& operator=(RayTracingParams const&) noexcept;
			RayTracingParams& operator=(RayTracingParams&&) noexcept;
			~RayTracingParams();

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

	public:
		// Raster batches:
		Batch(re::Lifetime, core::InvPtr<gr::MeshPrimitive> const&, EffectID); // No material; e.g. fullscreen quads, cubemap geo etc

		Batch(re::Lifetime,
			gr::MeshPrimitive::RenderData const&,
			gr::Material::MaterialInstanceRenderData const*,
			VertexStreamOverride const* = nullptr);

		Batch(re::Lifetime, RasterParams const&, EffectID, effect::drawstyle::Bitmask); // Custom (e.g. debug topology)

		// Compute batches:
		Batch(re::Lifetime, ComputeParams const&, EffectID);

		// Ray tracing batches:
		Batch(re::Lifetime, RayTracingParams const&);


	public:
		Batch(Batch&&) noexcept;
		Batch& operator=(Batch&&) noexcept;

		~Batch();

		
	public:		
		static Batch Duplicate(Batch const&, re::Lifetime);


	public:
		BatchType GetType() const;

		void SetEffectID(EffectID);
		EffectID GetEffectID() const;

		void Finalize(effect::drawstyle::Bitmask stageBitmask);

		core::InvPtr<re::Shader> const& GetShader() const;

		size_t GetInstanceCount() const;
		void SetInstanceCount(uint32_t numInstances);

		std::vector<BufferInput> const& GetBuffers() const;
		void SetBuffer(std::string_view shaderName, std::shared_ptr<re::Buffer> const&);
		void SetBuffer(std::string_view shaderName, std::shared_ptr<re::Buffer> const&, re::BufferView const&);
		void SetBuffer(re::BufferInput&&);
		void SetBuffer(re::BufferInput const&);

		void SetTextureInput(
			std::string_view shaderName,
			core::InvPtr<re::Texture> const&,
			core::InvPtr<re::Sampler> const&,
			re::TextureView const&);
		
		std::vector<TextureAndSamplerInput> const& GetTextureAndSamplerInputs() const;

		void SetRWTextureInput(
			std::string_view shaderName,
			core::InvPtr<re::Texture> const&,
			re::TextureView const&);

		std::vector<RWTextureInput> const& GetRWTextureInputs() const;

		void SetRootConstant(std::string_view shaderName, void const* src, re::DataType);
		RootConstants const& GetRootConstants() const;

		re::Lifetime GetLifetime() const;
	
		FilterBitmask GetBatchFilterMask() const;
		void SetFilterMaskBit(re::Batch::Filter filterBit, bool enabled);
		bool MatchesFilterBits(re::Batch::FilterBitmask required, re::Batch::FilterBitmask excluded) const;

		RasterParams const& GetRasterParams() const;
		ComputeParams const& GetComputeParams() const;
		RayTracingParams const& GetRayTracingParams() const;


	private:
		void ComputeDataHash() override;


	private:
		re::Lifetime m_lifetime;
		BatchType m_type;
		union
		{
			RasterParams m_rasterParams;
			ComputeParams m_computeParams;
			RayTracingParams m_rayTracingParams;
		};
		
		core::InvPtr<re::Shader> m_batchShader;

		EffectID m_effectID;
		effect::drawstyle::Bitmask m_drawStyleBitmask;
		FilterBitmask m_batchFilterBitmask;

		// Note: Batches can be responsible for the lifetime of a buffer held by a shared pointer: 
		// e.g. single-frame resources, or permanent buffers that are to be discarded (e.g. batch manager allocated a larger
		// one)
		std::vector<BufferInput> m_batchBuffers;

		std::vector<TextureAndSamplerInput> m_batchTextureSamplerInputs;
		std::vector<RWTextureInput> m_batchRWTextureInputs;

		RootConstants m_batchRootConstants;


	private:
		Batch(Batch const&) noexcept;
		Batch& operator=(Batch const&) noexcept;


	private:
		Batch() = delete;
	};


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