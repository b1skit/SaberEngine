#pragma once

#include "RenderStage.h"


using gr::Sampler;
using gr::Texture;
using std::string;
using std::shared_ptr;
using std::make_shared;
using glm::mat4;
using glm::mat3;
using glm::vec3;
using glm::vec4;


namespace gr
{
	RenderStage::RenderStage(std::string name) :
		m_name(name),
		m_textureTargetSet(name + " target"),
		m_stageGeometryBatches(nullptr)
		//m_stageInstancedGeometryBatches(nullptr)
	{
	}


	void RenderStage::SetTextureInput(
		string const& shaderName, shared_ptr<Texture const> tex, shared_ptr<Sampler const> sampler)
	{
		SEAssert("Stage shader is null. Set the stage shader before this call", m_stageShader != nullptr);
		SEAssert("Invalid shader sampler name", !shaderName.empty());
		SEAssert("Invalid texture", tex != nullptr);
		SEAssert("Invalid sampler", sampler != nullptr);
		
		SetPerFrameShaderUniformByPtr(
			shaderName,
			tex.get(),
			platform::Shader::UniformType::Texture,
			1);
		SetPerFrameShaderUniformByPtr(
			shaderName,
			sampler.get(),
			platform::Shader::UniformType::Sampler,
			1);
	}

	void RenderStage::SetPerFrameShaderUniformByPtr(
		string const& uniformName, void const* value, platform::Shader::UniformType const& type, int const count)
	{
		m_perFrameShaderUniforms.emplace_back(uniformName, value, type, count);
	}


	template <typename T>
	void RenderStage::SetPerFrameShaderUniformByValue(
		string const& uniformName, T const& value, platform::Shader::UniformType const& type, int const count)
	{
		// Dynamically allocate a copy of value so we have a pointer to it when we need for the current frame
		m_perFrameShaderUniformValues.emplace_back(std::make_shared<T>(value));
		SetPerFrameShaderUniformByPtr(
			uniformName, 
			m_perFrameShaderUniformValues.back().get(),
			type,
			count);
	}
	// Explicitely instantiate our templates so the compiler can link them from the .cpp file:
	template void RenderStage::SetPerFrameShaderUniformByValue<mat4>(
		string const& uniformName, mat4 const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerFrameShaderUniformByValue<mat3>(
		string const& uniformName, mat3 const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerFrameShaderUniformByValue<vec3>(
		string const& uniformName, vec3 const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerFrameShaderUniformByValue<vec4>(
		string const& uniformName, vec4 const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerFrameShaderUniformByValue<float>(
		string const& uniformName, float const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerFrameShaderUniformByValue<int>(
		string const& uniformName, int const& value, platform::Shader::UniformType const& type, int const count);


	void RenderStage::InitializeForNewFrame()
	{
		m_stageGeometryBatches = nullptr;
		m_perFrameShaderUniforms.clear();
		m_perFrameShaderUniformValues.clear();
		m_perMeshShaderUniforms.clear();
	}


	void RenderStage::SetPerMeshPerFrameShaderUniformByPtr(
		size_t meshIdx, std::string const& uniformName, void const* value, platform::Shader::UniformType const& type, 
		int const count)
	{
		SEAssert("meshIdx is OOB", meshIdx <= m_perMeshShaderUniforms.size());

		if (meshIdx == m_perMeshShaderUniforms.size())
		{
			m_perMeshShaderUniforms.emplace_back();
		}

		m_perMeshShaderUniforms[meshIdx].emplace_back(uniformName, value, type, count);
	}


	template <typename T>
	void RenderStage::SetPerMeshPerFrameShaderUniformByValue(
		size_t meshIdx, std::string const& uniformName, T const& value, platform::Shader::UniformType const& type, int const count)
	{
		// Dynamically allocate a copy of value so we have a pointer to it when we need for the current frame
		m_perFrameShaderUniformValues.emplace_back(std::make_shared<T>(value));

		SetPerMeshPerFrameShaderUniformByPtr(
			meshIdx,
			uniformName,
			m_perFrameShaderUniformValues.back().get(),
			type,
			count);
	}
	// Explicitely instantiate our templates so the compiler can link them from the .cpp file:
	template void RenderStage::SetPerMeshPerFrameShaderUniformByValue<mat4>(
		size_t meshIdx, string const& uniformName, mat4 const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerMeshPerFrameShaderUniformByValue<mat3>(
		size_t meshIdx, string const& uniformName, mat3 const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerMeshPerFrameShaderUniformByValue<vec3>(
		size_t meshIdx, string const& uniformName, vec3 const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerMeshPerFrameShaderUniformByValue<vec4>(
		size_t meshIdx, string const& uniformName, vec4 const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerMeshPerFrameShaderUniformByValue<float>(
		size_t meshIdx, string const& uniformName, float const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerMeshPerFrameShaderUniformByValue<int>(
		size_t meshIdx, string const& uniformName, int const& value, platform::Shader::UniformType const& type, int const count);
}