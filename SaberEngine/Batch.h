#pragma once

#include <vector>
#include <memory>

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
}


namespace re
{
	class Batch : public virtual en::HashedDataObject
	{
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
			GBufferWrite	= 0,
			ShadowCaster	= 1,

			Filter_Count
		};
		static_assert((uint32_t)re::Batch::Filter::Filter_Count < 32);

		struct ShaderUniform
		{
			std::string m_uniformName;
			std::shared_ptr<void> m_value;
			re::Shader::UniformType m_type;
			int m_count;
		};

	public:
		Batch(re::MeshPrimitive* meshPrimitive, gr::Material* material, re::Shader* shader);
		Batch(std::shared_ptr<gr::Mesh> const mesh, gr::Material* material, re::Shader* shader);

		~Batch() = default;
		Batch(Batch const&) = default;
		Batch(Batch&&) = default;
		Batch& operator=(Batch const&) = default;
		
		inline re::MeshPrimitive* GetBatchMesh() const { return m_batchMeshPrimitive; }
		inline gr::Material* GetBatchMaterial() const { return m_batchMaterial; }
		inline re::Shader* GetBatchShader() const { return m_batchShader; }

		inline size_t GetInstanceCount() const { return m_numInstances; }

		inline void AddBatchParameterBlock(std::shared_ptr<re::ParameterBlock> paramBlock) 
			{ m_batchParamBlocks.emplace_back(paramBlock); }
		inline std::vector<std::shared_ptr<re::ParameterBlock>> const& GetBatchParameterBlocks() const 
			{ return m_batchParamBlocks; }

		template <typename T>
		void AddBatchUniform(std::string const& uniformName, T const& value, re::Shader::UniformType const& type, int const count);
		inline std::vector<Batch::ShaderUniform> const& GetBatchUniforms() const { return m_batchUniforms; }

		inline uint32_t GetBatchFilterMask() const { return m_batchFilterMask; }
		void SetBatchFilterMaskBit(re::Batch::Filter filterBit);

		void IncrementBatchInstanceCount();

	private:
		void ComputeDataHash() override;

	private:
		re::MeshPrimitive* m_batchMeshPrimitive;
		gr::Material* m_batchMaterial;
		re::Shader* m_batchShader;

		std::vector<std::shared_ptr<re::ParameterBlock>> m_batchParamBlocks;

		std::vector<Batch::ShaderUniform> m_batchUniforms;

		GeometryMode m_batchGeometryMode;

		uint32_t m_batchFilterMask;

		size_t m_numInstances;

	private:
		Batch() = delete;
	};
}