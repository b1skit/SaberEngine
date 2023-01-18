// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "RenderStage.h"


namespace re
{
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


	RenderStage::RenderStage(std::string const& name)
		: NamedObject(name)
		, m_stageShader(nullptr)
		, m_textureTargetSet(nullptr)
		, m_stageCam(nullptr)
		, m_writesColor(true) // Reasonable assumption; Updated when we set the param block
		, m_batchFilterMask(0) // Accept all batches by default
	{
		m_textureTargetSet = std::make_shared<re::TextureTargetSet>(name + " target");
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


	void RenderStage::SetTextureTargetSet(std::shared_ptr<re::TextureTargetSet> targetSet)
	{
		SEAssert("Cannot set a null target set", targetSet != nullptr);
		m_textureTargetSet = targetSet;
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
			m_stageParams.m_colorWriteMode.R == re::Context::ColorWriteMode::ChannelMode::Enabled ||
			m_stageParams.m_colorWriteMode.G == re::Context::ColorWriteMode::ChannelMode::Enabled ||
			m_stageParams.m_colorWriteMode.B == re::Context::ColorWriteMode::ChannelMode::Enabled ||
			m_stageParams.m_colorWriteMode.A == re::Context::ColorWriteMode::ChannelMode::Enabled ? true : false;
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