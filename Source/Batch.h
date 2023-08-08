// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "HashedDataObject.h"
#include "Shader_Platform.h"


namespace gr
{
	class Material;
	class Mesh;
}
namespace re
{
	class MeshPrimitive;
	class ParameterBlock;
	class Shader;
	class Texture;
	class Sampler;
}


namespace re
{
	class Batch final : public virtual en::HashedDataObject
	{
	public:
		struct InstancedMeshParams
		{
			glm::mat4 g_model;
			static constexpr char const* const s_shaderName = "InstancedMeshParams"; // Not counted towards size of struct
		};

	public:
		enum class BatchType
		{
			Graphics,
			Compute
		};

		enum class GeometryMode
		{
			Indexed,
			IndexedInstanced,
			// TODO: Support other geometry draw modes

			GeometryMode_Count
		};

		enum class Filter
		{
			GBuffer_DoNotWrite	= 1 << 0,
			NoShadow			= 1 << 1,

			Filter_Count
		};
		static_assert((uint32_t)re::Batch::Filter::Filter_Count <= 32);

		// TODO: Combine with RenderStage::RenderStageTextureAndSamplerInput struct?
		static constexpr uint32_t k_allSubresources = std::numeric_limits<uint32_t>::max();
		struct BatchTextureAndSamplerInput
		{
			std::string m_shaderName;
			std::shared_ptr<re::Texture> m_texture;
			std::shared_ptr<re::Sampler> m_sampler;

			uint32_t m_subresource = k_allSubresources;
		};


		struct GraphicsParams
		{
			// TODO: Split this into vertex streams
			// -> TRICKY: OpenGL encapsulates state with VAOs, but we only need one VAO per mesh
			// -> Also need to pack the mesh draw mode on the batch (points/lines/triangles/etc)
			re::MeshPrimitive const* m_batchMeshPrimitive;
			
			GeometryMode m_batchGeometryMode;

			size_t m_numInstances;
		};
		struct ComputeParams
		{
			glm::uvec3 m_threadGroupCount = glm::uvec3(std::numeric_limits<uint32_t>::max());
		};

	public:
		// TODO: Switch batches to an object creation factory. We want them tightly packed in memory, so will need some
		// sort of allocation/memory management system

		Batch(re::MeshPrimitive const* meshPrimitive, gr::Material const* materialOverride);
		Batch(std::shared_ptr<gr::Mesh const> const mesh, gr::Material const* materialOverride);

		Batch(ComputeParams const& computeParams); // For compute batches

		~Batch() = default;
		Batch(Batch const&) = default;
		Batch(Batch&&) = default;
		Batch& operator=(Batch const&) = default;
		Batch& operator=(Batch&&) = default;
		
		re::MeshPrimitive const* GetMeshPrimitive() const;
		
		re::Shader const* GetShader() const;
		void SetShader(re::Shader*);

		size_t GetInstanceCount() const;

		std::vector<std::shared_ptr<re::ParameterBlock>> const& GetParameterBlocks() const;
		void SetParameterBlock(std::shared_ptr<re::ParameterBlock> paramBlock);

		void AddTextureAndSamplerInput(
			std::string const& shaderName, 
			std::shared_ptr<re::Texture>, 
			std::shared_ptr<re::Sampler>, 
			uint32_t subresource = k_allSubresources);
		std::vector<BatchTextureAndSamplerInput> const& GetTextureAndSamplerInputs() const;

		uint32_t GetBatchFilterMask() const;
		void SetFilterMaskBit(re::Batch::Filter filterBit);

		void IncrementBatchInstanceCount();

		GraphicsParams const& GetGraphicsParams() const;
		ComputeParams const& GetComputeParams() const;


	private:
		void ComputeDataHash() override;


	private:
		BatchType m_type;
		union
		{
			GraphicsParams m_graphicsParams;
			ComputeParams m_computeParams;
		};
		
		// TODO: Can any more of these be moved into graphics params?
		re::Shader const* m_batchShader;

		std::vector<std::shared_ptr<re::ParameterBlock>> m_batchParamBlocks;

		std::vector<BatchTextureAndSamplerInput> m_batchTextureSamplerInputs;

		uint32_t m_batchFilterMask;


	private:
		Batch() = delete;
	};


	inline re::MeshPrimitive const* Batch::GetMeshPrimitive() const
	{
		SEAssert("Invalid type", m_type == BatchType::Graphics);
		return m_graphicsParams.m_batchMeshPrimitive;
	}


	inline re::Shader const* Batch::GetShader() const
	{
		return m_batchShader;
	}


	inline void Batch::SetShader(re::Shader* shader)
	{
		SEAssert("Batch already has a shader. This is unexpected", m_batchShader == nullptr);
		m_batchShader = shader;
	}

	inline size_t Batch::GetInstanceCount() const
	{
		SEAssert("Invalid type", m_type == BatchType::Graphics);
		return m_graphicsParams.m_numInstances;
	}


	inline std::vector<std::shared_ptr<re::ParameterBlock>> const& Batch::GetParameterBlocks() const
	{
		return m_batchParamBlocks;
	}


	inline void Batch::SetParameterBlock(std::shared_ptr<re::ParameterBlock> paramBlock)
	{
		SEAssert("Cannot set a null parameter block", paramBlock != nullptr);
		m_batchParamBlocks.emplace_back(paramBlock);
	}


	inline std::vector<re::Batch::BatchTextureAndSamplerInput> const& Batch::GetTextureAndSamplerInputs() const
	{
		return m_batchTextureSamplerInputs;
	}


	inline uint32_t Batch::GetBatchFilterMask() const
	{
		return m_batchFilterMask;
	}


	inline Batch::GraphicsParams const& Batch::GetGraphicsParams() const
	{
		SEAssert("Invalid type", m_type == BatchType::Graphics);
		return m_graphicsParams;
	}


	inline Batch::ComputeParams const& Batch::GetComputeParams() const
	{
		SEAssert("Invalid type", m_type == BatchType::Compute);
		return m_computeParams;
	}
}