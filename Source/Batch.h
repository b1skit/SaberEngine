// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "HashedDataObject.h"
#include "MeshPrimitive.h"
#include "Shader_Platform.h"
#include "Texture.h"
#include "VertexStream.h"


namespace gr
{
	class Material;
	class Mesh;
}
namespace re
{
	class ParameterBlock;
	class Shader;
	class Sampler;
}


namespace re
{
	class Batch final : public virtual en::HashedDataObject
	{
	public:
		enum class Lifetime : bool
		{
			SingleFrame,
			Permanent			
		};
		static_assert(static_cast<bool>(Lifetime::SingleFrame) == static_cast<bool>(re::VertexStream::Lifetime::SingleFrame));
		static_assert(static_cast<bool>(Lifetime::Permanent) == static_cast<bool>(re::VertexStream::Lifetime::Permanent));

		enum class BatchType
		{
			Graphics,
			Compute
		};

		enum class GeometryMode
		{
			// Note: All draws are instanced, even if an API supports non-instanced drawing
			IndexedInstanced,
			ArrayInstanced
		};

		// Filter bits are exclusionary: A RenderStage will not draw a Batch if they have a matching filter bit
		enum class Filter : uint32_t
		{
			AlphaBlended		= 1 << 0,	// 0001
			NoShadow			= 1 << 1,	// 0010

			Filter_Count
		};
		static_assert((uint32_t)re::Batch::Filter::Filter_Count <= 32);

		// TODO: Combine with RenderStage::RenderStageTextureAndSamplerInput struct?
		struct BatchTextureAndSamplerInput
		{
			std::string m_shaderName;
			re::Texture const* m_texture;
			re::Sampler const* m_sampler;

			uint32_t m_srcMip = re::Texture::k_allMips;
		};


		struct GraphicsParams
		{
			// Don't forget to update ComputeDataHash if modifying this
			
			GeometryMode m_batchGeometryMode;
			uint32_t m_numInstances;
			gr::MeshPrimitive::TopologyMode m_batchTopologyMode;
			std::array<re::VertexStream const*, gr::MeshPrimitive::Slot_Count> m_vertexStreams;
			re::VertexStream const* m_indexStream;

			// If a batch is created via the CTOR that takes a gr::Material::MaterialInstanceData, we store the 
			// material's unique ID so we can include it in the data hash to ensure batches with identical geometry and
			// materials will sort together
			uint64_t m_materialUniqueID = std::numeric_limits<uint64_t>::max();
		};
		struct ComputeParams
		{
			// Don't forget to update ComputeDataHash if modifying this

			glm::uvec3 m_threadGroupCount = glm::uvec3(std::numeric_limits<uint32_t>::max());
		};

	public:
		// Graphics batches:
		Batch(Lifetime, gr::MeshPrimitive const* meshPrimitive); // No material; e.g. fullscreen quads, cubemap geo etc

		Batch(Lifetime, gr::MeshPrimitive::RenderData const& meshPrimRenderData, gr::Material::MaterialInstanceData const*);

		Batch(Lifetime, GraphicsParams const&); // e.g. debug topology

		// Compute batches:
		Batch(Lifetime, ComputeParams const& computeParams);

		~Batch() = default;
	
		Batch(Batch&&) = default;
		Batch& operator=(Batch&&) = default;
		
		static Batch Duplicate(Batch const&, re::Batch::Lifetime);

		BatchType GetType() const;
			
		re::Shader const* GetShader() const;
		void SetShader(re::Shader*);

		size_t GetInstanceCount() const;
		void SetInstanceCount(uint32_t numInstances);

		std::vector<std::shared_ptr<re::ParameterBlock>> const& GetParameterBlocks() const;
		void SetParameterBlock(std::shared_ptr<re::ParameterBlock> paramBlock);

		void AddTextureAndSamplerInput(
			char const* shaderName,
			re::Texture const*,
			re::Sampler const*,
			uint32_t srcMip = re::Texture::k_allMips);

		void AddTextureAndSamplerInput(
			char const* shaderName, 
			re::Texture const*, 
			std::shared_ptr<re::Sampler const>, 
			uint32_t srcMip = re::Texture::k_allMips);
		
		std::vector<BatchTextureAndSamplerInput> const& GetTextureAndSamplerInputs() const;

		Lifetime GetLifetime() const;
		uint32_t GetBatchFilterMask() const;
		void SetFilterMaskBit(re::Batch::Filter filterBit);

		GraphicsParams const& GetGraphicsParams() const;
		ComputeParams const& GetComputeParams() const;


	private:
		void ComputeDataHash() override;


	private:
		Lifetime m_lifetime;
		BatchType m_type;
		union
		{
			GraphicsParams m_graphicsParams;
			ComputeParams m_computeParams;
		};
		
		re::Shader const* m_batchShader;
		std::vector<std::shared_ptr<re::ParameterBlock>> m_batchParamBlocks;
		std::vector<BatchTextureAndSamplerInput> m_batchTextureSamplerInputs;
		uint32_t m_batchFilterBitmask;


	private: // Manually specify a lifetime for copies
		Batch(Batch const&) = default;
		Batch& operator=(Batch const&) = default;

	private:
		Batch() = delete;
	};


	inline re::Batch::BatchType Batch::GetType() const
	{
		return m_type;
	}


	inline re::Shader const* Batch::GetShader() const
	{
		return m_batchShader;
	}


	inline void Batch::SetShader(re::Shader* shader)
	{
		SEAssert(m_batchShader == nullptr, "Batch already has a shader. This is unexpected");
		m_batchShader = shader;
	}

	inline size_t Batch::GetInstanceCount() const
	{
		SEAssert(m_type == BatchType::Graphics, "Invalid type");
		return m_graphicsParams.m_numInstances;
	}


	inline std::vector<std::shared_ptr<re::ParameterBlock>> const& Batch::GetParameterBlocks() const
	{
		return m_batchParamBlocks;
	}


	inline std::vector<re::Batch::BatchTextureAndSamplerInput> const& Batch::GetTextureAndSamplerInputs() const
	{
		return m_batchTextureSamplerInputs;
	}


	inline Batch::Lifetime Batch::GetLifetime() const
	{
		return m_lifetime;
	}


	inline uint32_t Batch::GetBatchFilterMask() const
	{
		return m_batchFilterBitmask;
	}


	inline Batch::GraphicsParams const& Batch::GetGraphicsParams() const
	{
		SEAssert(m_type == BatchType::Graphics, "Invalid type");
		return m_graphicsParams;
	}


	inline Batch::ComputeParams const& Batch::GetComputeParams() const
	{
		SEAssert(m_type == BatchType::Compute, "Invalid type");
		return m_computeParams;
	}
}