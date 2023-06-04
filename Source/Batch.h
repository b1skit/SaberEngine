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

		typedef std::vector<std::tuple<std::string, std::shared_ptr<re::Texture>, std::shared_ptr<re::Sampler>>> BatchTextureAndSamplerInput;


	public:
		Batch(re::MeshPrimitive const* meshPrimitive, gr::Material const* materialOverride);
		Batch(std::shared_ptr<gr::Mesh const> const mesh, gr::Material const* materialOverride);

		~Batch() = default;
		Batch(Batch const&) = default;
		Batch(Batch&&) = default;
		Batch& operator=(Batch const&) = default;
		
		re::MeshPrimitive const* GetMeshPrimitive() const;
		
		re::Shader const* GetShader() const;
		void SetShader(re::Shader*);

		size_t GetInstanceCount() const;

		std::vector<std::shared_ptr<re::ParameterBlock>> const& GetParameterBlocks() const;
		void SetParameterBlock(std::shared_ptr<re::ParameterBlock> paramBlock);

		void AddTextureAndSamplerInput(
			std::string const& shaderName, std::shared_ptr<re::Texture>, std::shared_ptr<re::Sampler>);
		BatchTextureAndSamplerInput const& GetTextureAndSamplerInputs() const;

		uint32_t GetBatchFilterMask() const;
		void SetFilterMaskBit(re::Batch::Filter filterBit);

		void IncrementBatchInstanceCount();


	private:
		void ComputeDataHash() override;


	private:
		// TODO: Split this into vertex streams
		// -> TRICKY: OpenGL encapsulates state with VAOs, but we only need one VAO per mesh
		// -> Also need to pack the mesh draw mode on the batch (points/lines/triangles/etc)
		re::MeshPrimitive const* m_batchMeshPrimitive;

		re::Shader const* m_batchShader;

		std::vector<std::shared_ptr<re::ParameterBlock>> m_batchParamBlocks;

		BatchTextureAndSamplerInput m_batchTextureSamplerInputs;

		GeometryMode m_batchGeometryMode;

		uint32_t m_batchFilterMask;

		size_t m_numInstances;

	private:
		Batch() = delete;
	};


	inline re::MeshPrimitive const* Batch::GetMeshPrimitive() const
	{
		return m_batchMeshPrimitive;
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
		return m_numInstances;
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


	inline Batch::BatchTextureAndSamplerInput const& Batch::GetTextureAndSamplerInputs() const
	{
		return m_batchTextureSamplerInputs;
	}


	inline uint32_t Batch::GetBatchFilterMask() const
	{
		return m_batchFilterMask;
	}
}