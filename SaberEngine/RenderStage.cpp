#pragma once

#include "RenderStage.h"


using re::Sampler;
using re::Texture;
using std::string;
using std::shared_ptr;
using std::make_shared;
using std::vector;
using glm::mat4;
using glm::mat3;
using glm::vec3;
using glm::vec4;


namespace re
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

		m_perFrameParamBlocks = rhs.m_perFrameParamBlocks;
		m_permanentParamBlocks = rhs.m_permanentParamBlocks;

		m_stageBatches = rhs.m_stageBatches;
	}


	void RenderStage::SetTextureInput(
		string const& shaderName, shared_ptr<Texture> tex, shared_ptr<Sampler> sampler)
	{
		SEAssert("Stage shader is null. Set the stage shader before this call", m_stageShader != nullptr);
		SEAssert("Invalid shader sampler name", !shaderName.empty());
		SEAssert("Invalid texture", tex != nullptr);
		SEAssert("Invalid sampler", sampler != nullptr);

		// Hold a copy of our shared pointers to ensure they don't go out of scope until we're done with them:
		m_perFrameShaderUniformValues.emplace_back(std::static_pointer_cast<void>(tex));
		m_perFrameShaderUniformValues.emplace_back(std::static_pointer_cast<void>(sampler));

		// Add our raw pointers to the list of StageShaderUniforms:
		m_perFrameShaderUniforms.emplace_back(shaderName, tex.get(), re::Shader::UniformType::Texture, 1);
		m_perFrameShaderUniforms.emplace_back(shaderName, sampler.get(), re::Shader::UniformType::Sampler, 1);
	}
	

	void RenderStage::EndOfFrame()
	{
		m_perFrameShaderUniforms.clear();
		m_perFrameShaderUniformValues.clear();
		m_perFrameParamBlocks.clear();
		m_stageBatches.clear();
	}


	void RenderStage::SetStagePipelineStateParams(PipelineStateParams const& params)
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


	void RenderStage::AddPermanentParameterBlock(std::shared_ptr<re::ParameterBlock> pb)
	{
		m_permanentParamBlocks.emplace_back(pb);
	}


	void RenderStage::AddSingleFrameParameterBlock(std::shared_ptr<re::ParameterBlock> pb)
	{
		m_perFrameParamBlocks.emplace_back(pb);
	}
}