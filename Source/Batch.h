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
		Batch(re::MeshPrimitive* meshPrimitive, gr::Material* material);
		Batch(std::shared_ptr<gr::Mesh> const mesh, gr::Material* material);

		~Batch() = default;
		Batch(Batch const&) = default;
		Batch(Batch&&) = default;
		Batch& operator=(Batch const&) = default;
		
		inline re::MeshPrimitive* GetBatchMesh() const { return m_batchMeshPrimitive; }
		inline re::Shader* GetBatchShader() const { return m_batchShader; }

		inline size_t GetInstanceCount() const { return m_numInstances; }

		inline void AddBatchParameterBlock(std::shared_ptr<re::ParameterBlock> paramBlock) 
			{ m_batchParamBlocks.emplace_back(paramBlock); }
		inline std::vector<std::shared_ptr<re::ParameterBlock>> const& GetBatchParameterBlocks() const 
			{ return m_batchParamBlocks; }

		void AddBatchTextureAndSamplerInput(
			std::string const& shaderName, std::shared_ptr<re::Texture> texture, std::shared_ptr<re::Sampler> sampler);
		inline BatchTextureAndSamplerInput const& GetBatchTextureAndSamplerInputs() const { return m_batchTextureSamplerInputs; }

		inline uint32_t GetBatchFilterMask() const { return m_batchFilterMask; }
		void SetBatchFilterMaskBit(re::Batch::Filter filterBit);

		void IncrementBatchInstanceCount();

	private:
		void ComputeDataHash() override;

	private:
		// TODO: Split this into vertex streams
		// -> TRICKY: OpenGL encapsulates state with VAOs, but we only need one VAO per mesh
		// -> Also need to pack the mesh draw mode on the batch (points/lines/triangles/etc)
		re::MeshPrimitive* m_batchMeshPrimitive;

		re::Shader* m_batchShader;

		std::vector<std::shared_ptr<re::ParameterBlock>> m_batchParamBlocks;

		BatchTextureAndSamplerInput m_batchTextureSamplerInputs;

		GeometryMode m_batchGeometryMode;

		uint32_t m_batchFilterMask;

		size_t m_numInstances;

	private:
		Batch() = delete;
	};
}