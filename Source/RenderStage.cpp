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
		, m_writesColor(true) // Reasonable assumption; Updated when we set the pipeline state
		, m_batchFilterMask(0) // Accept all batches by default
	{
		m_textureTargetSet = std::make_shared<re::TextureTargetSet>(name + " target");
	}


	RenderStage::RenderStage(RenderStage const& rhs) 
		: RenderStage(rhs.GetName())
	{
		m_stageShader = rhs.m_stageShader;
		m_textureTargetSet = rhs.m_textureTargetSet;
		m_pipelineState = rhs.m_pipelineState;
		m_writesColor = rhs.m_writesColor;

		m_perFrameTextureSamplerInputs = rhs.m_perFrameTextureSamplerInputs;
		m_perFrameParamBlocks = rhs.m_perFrameParamBlocks;

		m_permanentParamBlocks = rhs.m_permanentParamBlocks;

		m_stageBatches = rhs.m_stageBatches;
	}


	void RenderStage::SetTextureTargetSet(std::shared_ptr<re::TextureTargetSet> targetSet)
	{
		m_textureTargetSet = targetSet;
	}


	void RenderStage::SetPerFrameTextureInput(
		string const& shaderName, shared_ptr<Texture> tex, shared_ptr<Sampler> sampler)
	{
		SEAssert("Stage shader is null. Set the stage shader before this call", m_stageShader != nullptr);
		SEAssert("Invalid shader sampler name", !shaderName.empty());
		SEAssert("Invalid texture", tex != nullptr);
		SEAssert("Invalid sampler", sampler != nullptr);

		m_perFrameTextureSamplerInputs.emplace_back(shaderName, tex, sampler);
	}
	

	void RenderStage::EndOfFrame()
	{
		m_perFrameTextureSamplerInputs.clear();
		m_perFrameParamBlocks.clear();
		m_stageBatches.clear();
	}


	void RenderStage::SetStagePipelineState(gr::PipelineState const& params)
	{
		m_pipelineState = params;

		m_writesColor =
			m_pipelineState.m_colorWriteMode.R == gr::PipelineState::ColorWriteMode::ChannelMode::Enabled ||
			m_pipelineState.m_colorWriteMode.G == gr::PipelineState::ColorWriteMode::ChannelMode::Enabled ||
			m_pipelineState.m_colorWriteMode.B == gr::PipelineState::ColorWriteMode::ChannelMode::Enabled ||
			m_pipelineState.m_colorWriteMode.A == gr::PipelineState::ColorWriteMode::ChannelMode::Enabled ? true : false;
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