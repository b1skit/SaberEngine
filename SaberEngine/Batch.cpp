#include "Batch.h"
#include "MeshPrimitive.h"
#include "Material.h"
#include "Shader.h"
#include "ParameterBlock.h"
#include "Mesh.h"

using std::string;
using std::shared_ptr;
using std::make_shared;

namespace
{
	constexpr size_t k_batchParamBlockIDsReserveAmount = 10;
}


namespace re
{
	Batch::Batch(re::MeshPrimitive* meshPrimitive, gr::Material const* material, gr::Shader const* shader) :	
		m_batchMeshPrimitive(meshPrimitive),
		m_batchMaterial(material),
		m_batchShader(shader),
		m_batchGeometryMode(GeometryMode::Indexed),
		m_batchFilterMask(0),
		m_numInstances(1)
	{
		m_paramBlocks.reserve(k_batchParamBlockIDsReserveAmount);

		// Material params:
		if (material)
		{
			m_paramBlocks.emplace_back(material->GetParameterBlock());
		}
		
		ComputeDataHash();
	}


	Batch::Batch(std::shared_ptr<gr::Mesh> const mesh, gr::Material const* material, gr::Shader const* shader)
		: Batch(mesh->GetMeshPrimitives()[0].get(), material, shader) // Delegating ctor
	{
			SEAssert("Currently only support Mesh with a single MeshPrimitve. TODO: Support > 1 MeshPrimitve", 
			mesh->GetMeshPrimitives().size() == 1);
	}


	void Batch::IncrementBatchInstanceCount()
	{
		// Update the batch draw mode to be an indexed type:
		switch (m_batchGeometryMode)
		{
		case GeometryMode::Indexed:
		{
			m_batchGeometryMode = GeometryMode::IndexedInstanced;
		}
		break;
		default:
			break;
		}

		m_numInstances++;
	}


	void Batch::ComputeDataHash()
	{
		// Batch filter mask bit:
		AddDataBytesToHash(m_batchFilterMask);

		// MeshPrimitive data:
		SEAssert("Batch must have a valid MeshPrimitive", m_batchMeshPrimitive);
		AddDataBytesToHash(m_batchMeshPrimitive->GetDataHash());
		
		// Material:
		if (m_batchMaterial)
		{
			AddDataBytesToHash(&m_batchMaterial->GetName()[0], m_batchMaterial->GetName().length() * sizeof(char));
		}

		// Shader:
		if (m_batchShader)
		{
			AddDataBytesToHash(&m_batchShader->GetName()[0], m_batchShader->GetName().length() * sizeof(char));
		}

		// Parameter blocks:
		for (size_t i = 0; i < m_paramBlocks.size(); i++)
		{
			AddDataBytesToHash(m_paramBlocks[i]);
		}

		// Note: We don't compute hashes for any batch uniforms here; they're appended as they're added to the batch
	}


	template <typename T>
	void Batch::AddBatchUniform(
		string const& uniformName, T const& value, platform::Shader::UniformType const& type, int const count)
	{
		SEAssert("TODO: Support count > 1", count == 1);

		// Store the shared_ptr for textures/samplers, or copy the data for other types
		if (type == platform::Shader::UniformType::Texture || type == platform::Shader::UniformType::Sampler)
		{
			SEAssert("Invalid pointer type",
				typeid(T) == typeid(shared_ptr<gr::Texture>) || typeid(T) == typeid(shared_ptr<gr::Sampler>));
			SEAssert("Pointer is null", std::static_pointer_cast<const void>(value) != nullptr);

			m_batchUniforms.emplace_back(uniformName, value, type, count);

			// Add the pointer value to the hash; Should be safe as we manage them via shared_ptrs & don't allow copying
			AddDataBytesToHash(std::static_pointer_cast<const void>(value).get());
		}
		else
		{
			m_batchUniforms.emplace_back(uniformName, make_shared<T>(value), type, count);

			// Add the reference address to the hash; Risky, as this could be anything, but will allow instancing IF
			// the value has a consistent memory location...
			AddDataBytesToHash(&value);
		}
	}
	// Explicitely instantiate our templates so the compiler can link them from the .cpp file:
	template void Batch::AddBatchUniform<shared_ptr<gr::Texture>>(
		string const& uniformName, shared_ptr<gr::Texture> const& value, platform::Shader::UniformType const& type, int const count);
	template void Batch::AddBatchUniform<shared_ptr<gr::Sampler>>(
		string const& uniformName, shared_ptr<gr::Sampler> const& value, platform::Shader::UniformType const& type, int const count);


	void Batch::SetBatchFilterMaskBit(Filter filterBit)
	{
		m_batchFilterMask |= (1 << (uint32_t)filterBit);
	}
}