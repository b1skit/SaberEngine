#pragma once

#include "RenderStage.h"


using gr::Sampler;
using gr::Texture;
using std::string;
using std::shared_ptr;
using std::make_shared;
using std::vector;
using glm::mat4;
using glm::mat3;
using glm::vec3;
using glm::vec4;


namespace gr
{
	RenderStage::RenderStage(std::string const& name) :
			NamedObject(name),
		m_textureTargetSet(name + " target"),
		m_writesColor(true), // Reasonable assumption; Updated when we set the param block
		m_batchFilterMask(0) // Accept all batches by default
	{
	}


	RenderStage::RenderStage(RenderStage const& rhs) : RenderStage(rhs.GetName())
	{
		m_stageShader = rhs.m_stageShader;
		m_textureTargetSet = rhs.m_textureTargetSet;
		m_stageCam = rhs.m_stageCam;
		m_stageParams = rhs.m_stageParams;
		m_writesColor = rhs.m_writesColor;

		m_perFrameShaderUniforms = vector<StageShaderUniform>(rhs.m_perFrameShaderUniforms);
		m_perFrameShaderUniformValues = rhs.m_perFrameShaderUniformValues;

		m_stageParamBlocks = rhs.m_stageParamBlocks;

		m_stageBatches = rhs.m_stageBatches;
	}


	void RenderStage::SetTextureInput(
		string const& shaderName, shared_ptr<Texture const> tex, shared_ptr<Sampler const> sampler)
	{
		SEAssert("Stage shader is null. Set the stage shader before this call", m_stageShader != nullptr);
		SEAssert("Invalid shader sampler name", !shaderName.empty());
		SEAssert("Invalid texture", tex != nullptr);
		SEAssert("Invalid sampler", sampler != nullptr);

		// Hold a copy of our shared pointers to ensure they don't go out of scope until we're done with them:
		m_perFrameShaderUniformValues.emplace_back(std::static_pointer_cast<const void>(tex));
		m_perFrameShaderUniformValues.emplace_back(std::static_pointer_cast<const void>(sampler));

		// Add our raw pointers to the list of StageShaderUniforms:
		m_perFrameShaderUniforms.emplace_back(shaderName, tex.get(), platform::Shader::UniformType::Texture, 1);
		m_perFrameShaderUniforms.emplace_back(shaderName, sampler.get(), platform::Shader::UniformType::Sampler, 1);
	}


	template <typename T>
	void RenderStage::SetPerFrameShaderUniform(
		string const& uniformName, T const& value, platform::Shader::UniformType const& type, int const count)
	{
		// Dynamically allocate a copy of value so we have a pointer to it when we need for the current frame
		m_perFrameShaderUniformValues.emplace_back(std::make_shared<T>(value));

		void const* valuePtr;
		if (count > 1)
		{
			// Assume if count > 1, we've recieved multiple values packed into a std::vector. 
			// Thus, we must store the address of the first element of the vector (NOT the address of the vector object!)
			valuePtr = &(reinterpret_cast<vector<T> const*>(m_perFrameShaderUniformValues.back().get())->at(0));
		}
		else
		{
			valuePtr = m_perFrameShaderUniformValues.back().get();
		}

		m_perFrameShaderUniforms.emplace_back(uniformName, valuePtr, type, count);
	}
	// Explicitely instantiate our templates so the compiler can link them from the .cpp file:
	template void RenderStage::SetPerFrameShaderUniform<mat4>(
		string const& uniformName, mat4 const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerFrameShaderUniform<vector<mat4>>(
		string const& uniformName, vector<mat4> const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerFrameShaderUniform<mat3>(
		string const& uniformName, mat3 const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerFrameShaderUniform<vec3>(
		string const& uniformName, vec3 const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerFrameShaderUniform<vec4>(
		string const& uniformName, vec4 const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerFrameShaderUniform<float>(
		string const& uniformName, float const& value, platform::Shader::UniformType const& type, int const count);
	template void RenderStage::SetPerFrameShaderUniform<int>(
		string const& uniformName, int const& value, platform::Shader::UniformType const& type, int const count);


	void RenderStage::InitializeForNewFrame()
	{
		m_perFrameShaderUniforms.clear();
		m_perFrameShaderUniformValues.clear();
		m_stageBatches.clear();
	}


	void RenderStage::SetRenderStageParams(RenderStageParams const& params)
	{
		m_stageParams = params;

		m_writesColor =
			m_stageParams.m_colorWriteMode.R == platform::Context::ColorWriteMode::ChannelMode::Enabled ||
			m_stageParams.m_colorWriteMode.G == platform::Context::ColorWriteMode::ChannelMode::Enabled ||
			m_stageParams.m_colorWriteMode.B == platform::Context::ColorWriteMode::ChannelMode::Enabled ||
			m_stageParams.m_colorWriteMode.A == platform::Context::ColorWriteMode::ChannelMode::Enabled ? true : false;
	}


	void RenderStage::AddBatches(std::vector<re::Batch> const& batches)
	{
		for (size_t i = 0; i < batches.size(); i++)
		{
			AddBatch(batches[i]); // Checks filter mask bit before accepting the batch
		}
	}


	void RenderStage::AddBatch(re::Batch const& batch)
	{
		if (m_batchFilterMask & batch.GetBatchFilterMask() || !m_batchFilterMask) // Accept all batches by default
		{
			m_stageBatches.emplace_back(batch);
		}
	}


	void RenderStage::SetBatchFilterMaskBit(re::Batch::Filter filterBit)
	{
		m_batchFilterMask |= (1 << (uint32_t)filterBit);
	}


	std::shared_ptr<re::ParameterBlock> RenderStage::GetPermanentParameterBlock(std::string const& pbShaderName) const
	{
		const uint64_t hashedName = en::NamedObject::ComputeIDFromName(pbShaderName);

		// TODO: This should not be a linear search. We should use an unordered_map instead
		for (size_t i = 0; i < m_stageParamBlocks.size(); i++)
		{
			if (m_stageParamBlocks[i]->GetNameID() == hashedName)
			{
				return m_stageParamBlocks[i];
			}
		}

		SEAssert("Could not find a parameter block with the given shader name", false);
		return nullptr;
	}
}