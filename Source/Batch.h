// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Effect.h"
#include "MeshPrimitive.h"
#include "Shader_Platform.h"
#include "Texture.h"
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
	class Shader;
	class Sampler;
}


namespace re
{
	struct TextureAndSamplerInput
	{
		std::string m_shaderName;
		re::Texture const* m_texture;
		re::Sampler const* m_sampler;

		uint32_t m_srcArrayElement = re::Texture::k_allArrayElements;
		uint32_t m_srcFace = re::Texture::k_allFaces;
		uint32_t m_srcMip = re::Texture::k_allMips;
	};


	class Batch final : public virtual core::IHashedDataObject
	{
	public:
		enum class Lifetime : bool
		{
			SingleFrame,
			Permanent			
		};
		SEStaticAssert(
			static_cast<bool>(Lifetime::SingleFrame) == static_cast<bool>(re::VertexStream::Lifetime::SingleFrame) &&
			static_cast<bool>(Lifetime::Permanent) == static_cast<bool>(re::VertexStream::Lifetime::Permanent),
			"Batch and VertexStream Lifetime enums are out of sync");

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

		using FilterBitmask = uint32_t;
		enum Filter : FilterBitmask
		{
			AlphaBlended		= 1 << 0,	// 0001
			CastsShadow			= 1 << 1,	// ...

			Filter_Count
		};
		SEStaticAssert(re::Batch::Filter::Filter_Count <= 32, "Too many filter bits");


		struct GraphicsParams
		{
			// Note: Don't forget to update ComputeDataHash() if modifying this
			
			GeometryMode m_batchGeometryMode;
			uint32_t m_numInstances;
			gr::MeshPrimitive::TopologyMode m_batchTopologyMode;

			std::array<re::VertexStream const*, gr::MeshPrimitive::Slot_Count> m_vertexStreams;
			re::VertexStream const* m_indexStream;

			// If a batch is created via the CTOR that takes a gr::Material::MaterialInstanceData, we store the 
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
		Batch(Lifetime, gr::MeshPrimitive const*, EffectID); // No material; e.g. fullscreen quads, cubemap geo etc

		Batch(Lifetime, gr::MeshPrimitive::RenderData const&, gr::Material::MaterialInstanceData const*);

		Batch(Lifetime, GraphicsParams const&, EffectID); // e.g. debug topology

		// Compute batches:
		Batch(Lifetime, ComputeParams const&, EffectID);


	public:
		~Batch() = default;
	
		Batch(Batch&&) = default;
		Batch& operator=(Batch&&) = default;


	public:		
		static Batch Duplicate(Batch const&, re::Batch::Lifetime);


	public:
		BatchType GetType() const;

		void SetEffectID(EffectID);
		EffectID GetEffectID() const;

		void ResolveShader(effect::DrawStyle::Bitmask stageBitmask);

		re::Shader const* GetShader() const;

		size_t GetInstanceCount() const;
		void SetInstanceCount(uint32_t numInstances);

		std::vector<std::shared_ptr<re::Buffer>> const& GetBuffers() const;
		void SetBuffer(std::shared_ptr<re::Buffer>);

		void AddTextureAndSamplerInput(
			char const* shaderName,
			re::Texture const*,
			re::Sampler const*,
			uint32_t arrayElement = re::Texture::k_allArrayElements,
			uint32_t faceIdx = re::Texture::k_allFaces,
			uint32_t mipLevel = re::Texture::k_allMips);

		void AddTextureAndSamplerInput(
			char const* shaderName, 
			std::shared_ptr<re::Texture const>,
			std::shared_ptr<re::Sampler const>,
			uint32_t arrayElement = re::Texture::k_allArrayElements,
			uint32_t faceIdx = re::Texture::k_allFaces,
			uint32_t mipLevel = re::Texture::k_allMips);
		
		std::vector<TextureAndSamplerInput> const& GetTextureAndSamplerInputs() const;

		Lifetime GetLifetime() const;
		
		FilterBitmask GetBatchFilterMask() const;
		void SetFilterMaskBit(re::Batch::Filter filterBit, bool enabled);
		bool MatchesFilterBits(re::Batch::FilterBitmask required, re::Batch::FilterBitmask excluded) const;

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

		EffectID m_effectID;
		effect::DrawStyle::Bitmask m_drawStyleBitmask;
		FilterBitmask m_batchFilterBitmask;

		// Note: Batches can be responsible for the lifetime of a buffer held by a shared pointer: 
		// e.g. single-frame resources, or permanent buffers that are to be discarded (e.g. batch manager allocated a larger
		// one)
		std::vector<std::shared_ptr<re::Buffer>> m_batchBuffers;

		std::vector<TextureAndSamplerInput> m_batchTextureSamplerInputs;


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


	inline std::vector<std::shared_ptr<re::Buffer>> const& Batch::GetBuffers() const
	{
		return m_batchBuffers;
	}


	inline std::vector<re::TextureAndSamplerInput> const& Batch::GetTextureAndSamplerInputs() const
	{
		return m_batchTextureSamplerInputs;
	}


	inline Batch::Lifetime Batch::GetLifetime() const
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