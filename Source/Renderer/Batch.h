// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Effect.h"
#include "EnumTypes.h"
#include "MeshPrimitive.h"
#include "Shader_Platform.h"
#include "TextureView.h"
#include "VertexStream.h"

#include "Core/Interfaces/IHashedDataObject.h"


namespace gr
{
	class Material;
	class Mesh;
}
namespace re
{
	class Buffer;
	class BufferInput;
	class Shader;
	class Sampler;
	class Texture;
}


namespace re
{
	class Batch final : public virtual core::IHashedDataObject
	{
	public:
		enum class BatchType
		{
			Graphics,
			Compute
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
			AlphaBlended		= 1 << 0,	// 0001
			CastsShadow			= 1 << 1,	// ...

			Filter_Count
		};
		SEStaticAssert(re::Batch::Filter::Filter_Count <= 32, "Too many filter bits");


		struct VertexStreamInput
		{
			static constexpr uint8_t k_invalidSlotIdx = std::numeric_limits<uint8_t>::max();

			re::VertexStream const* m_vertexStream = nullptr;
			uint8_t m_bindSlot = k_invalidSlotIdx; // NOTE: Automatically resolved by the batch
		};
		struct GraphicsParams
		{
			// Note: Don't forget to update ComputeDataHash() if modifying this
			
			GeometryMode m_batchGeometryMode = GeometryMode::Invalid;
			uint32_t m_numInstances = 0;
			gr::MeshPrimitive::PrimitiveTopology m_primitiveTopology = gr::MeshPrimitive::PrimitiveTopology::TriangleList;

			VertexStreamInput m_vertexStreams[re::VertexStream::k_maxVertexStreams] = {0};
			uint8_t m_numVertexStreams = 0;

			re::VertexStream const* m_indexStream = nullptr;

			// If a batch is created via the CTOR that takes a gr::Material::MaterialInstanceRenderData, we store the 
			// material's unique ID so we can include it in the data hash to ensure batches with identical geometry and
			// materials will sort together
			uint64_t m_materialUniqueID = core::INamedObject::k_invalidUniqueID;
		};
		struct ComputeParams
		{
			// Note: Don't forget to update ComputeDataHash() if modifying this

			glm::uvec3 m_threadGroupCount = glm::uvec3(std::numeric_limits<uint32_t>::max());
		};

	public:
		// Graphics batches:
		Batch(re::Lifetime, gr::MeshPrimitive const*, EffectID); // No material; e.g. fullscreen quads, cubemap geo etc

		Batch(re::Lifetime, gr::MeshPrimitive::RenderData const&, gr::Material::MaterialInstanceRenderData const*);

		Batch(re::Lifetime, GraphicsParams const&, EffectID, effect::drawstyle::Bitmask); // e.g. debug topology

		// Compute batches:
		Batch(re::Lifetime, ComputeParams const&, EffectID);


	public:
		~Batch() = default;
	
		Batch(Batch&&) = default;
		Batch& operator=(Batch&&) = default;


	public:		
		static Batch Duplicate(Batch const&, re::Lifetime);


	public:
		BatchType GetType() const;

		void SetEffectID(EffectID);
		EffectID GetEffectID() const;

		void ResolveShader(effect::drawstyle::Bitmask stageBitmask);

		re::Shader const* GetShader() const;

		size_t GetInstanceCount() const;
		void SetInstanceCount(uint32_t numInstances);

		std::vector<BufferInput> const& GetBuffers() const;
		void SetBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer>);
		void SetBuffer(re::BufferInput const&);
		void SetBuffer(re::BufferInput&&);


		void AddTextureInput(
			char const* shaderName,
			re::Texture const*,
			re::Sampler const*,
			re::TextureView const&);

		void AddTextureInput(
			char const* shaderName, 
			std::shared_ptr<re::Texture const>,
			std::shared_ptr<re::Sampler const>,
			re::TextureView const&);
		
		std::vector<TextureAndSamplerInput> const& GetTextureAndSamplerInputs() const;

		void AddRWTextureInput(
			char const* shaderName,
			re::Texture const*,
			re::TextureView const&);

		void AddRWTextureInput(
			char const* shaderName,
			std::shared_ptr<re::Texture const>,
			re::TextureView const&);

		std::vector<RWTextureInput> const& GetRWTextureInputs() const;

		re::Lifetime GetLifetime() const;
	
		FilterBitmask GetBatchFilterMask() const;
		void SetFilterMaskBit(re::Batch::Filter filterBit, bool enabled);
		bool MatchesFilterBits(re::Batch::FilterBitmask required, re::Batch::FilterBitmask excluded) const;

		GraphicsParams const& GetGraphicsParams() const;
		ComputeParams const& GetComputeParams() const;


	private:
		void ComputeDataHash() override;


	private:
		re::Lifetime m_lifetime;
		BatchType m_type;
		union
		{
			GraphicsParams m_graphicsParams;
			ComputeParams m_computeParams;
		};
		
		re::Shader const* m_batchShader;

		EffectID m_effectID;
		effect::drawstyle::Bitmask m_drawStyleBitmask;
		FilterBitmask m_batchFilterBitmask;

		// Note: Batches can be responsible for the lifetime of a buffer held by a shared pointer: 
		// e.g. single-frame resources, or permanent buffers that are to be discarded (e.g. batch manager allocated a larger
		// one)
		std::vector<BufferInput> m_batchBuffers;

		std::vector<TextureAndSamplerInput> m_batchTextureSamplerInputs;
		std::vector<RWTextureInput> m_batchRWTextureInputs;


	private:
		Batch(Batch const&) = default;
		Batch& operator=(Batch const&) = default;

	private:
		Batch() = delete;
	};


	inline re::Batch::BatchType Batch::GetType() const
	{
		return m_type;
	}


	inline void Batch::SetEffectID(EffectID effectID)
	{
		SEAssert(m_effectID == effect::Effect::k_invalidEffectID, "EffectID has already been set. This is unexpected");
		m_effectID = effectID;
	}


	inline EffectID Batch::GetEffectID() const
	{
		return m_effectID;
	}


	inline re::Shader const* Batch::GetShader() const
	{
		return m_batchShader;
	}


	inline size_t Batch::GetInstanceCount() const
	{
		SEAssert(m_type == BatchType::Graphics, "Invalid type");
		return m_graphicsParams.m_numInstances;
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


	inline re::Lifetime Batch::GetLifetime() const
	{
		return m_lifetime;
	}


	inline re::Batch::FilterBitmask Batch::GetBatchFilterMask() const
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