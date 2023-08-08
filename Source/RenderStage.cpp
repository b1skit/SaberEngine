// © 2022 Adam Badke. All rights reserved.
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
	std::shared_ptr<RenderStage> RenderStage::CreateGraphicsStage(
		std::string const& name, GraphicsStageParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newGFXStage;
		newGFXStage.reset(new RenderStage(
			name, 
			std::make_unique<GraphicsStageParams>(stageParams), 
			RenderStage::RenderStageType::Graphics));
		return newGFXStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateComputeStage(
		std::string const& name, ComputeStageParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newComputeStage;
		newComputeStage.reset(new ComputeStage(
			name, 
			std::make_unique<ComputeStageParams>(stageParams)));
		return newComputeStage;
	}


	RenderStage::RenderStage(
		std::string const& name, std::unique_ptr<IStageParams>&& stageParams, RenderStageType stageType)
		: NamedObject(name)
		, m_type(stageType)
		, m_stageParams(nullptr)
		, m_stageShader(nullptr)
		, m_textureTargetSet(nullptr)
		, m_batchFilterMask(0)	// Accept all batches by default
	{
		SEAssert("Invalid RenderStage name", !GetName().empty());

		m_stageParams = std::move(stageParams);
	}


	ComputeStage::ComputeStage(std::string const& name, std::unique_ptr<ComputeStageParams>&& stageParams)
		: NamedObject(name)
		, RenderStage(name, std::move(stageParams), RenderStageType::Compute)
	{
	}


	void RenderStage::SetTextureTargetSet(std::shared_ptr<re::TextureTargetSet> targetSet)
	{
		m_textureTargetSet = targetSet;
	}


	void RenderStage::SetPerFrameTextureInput(
		string const& shaderName, shared_ptr<Texture> tex, shared_ptr<Sampler> sampler, uint32_t subresource /*= k_allSubresources*/)
	{
		SEAssert("Stage shader is null. Set the stage shader before this call", m_stageShader != nullptr);
		SEAssert("Invalid shader sampler name", !shaderName.empty());
		SEAssert("Invalid texture", tex != nullptr);
		SEAssert("Invalid sampler", sampler != nullptr);

		m_perFrameTextureSamplerInputs.emplace_back(RenderStageTextureAndSamplerInput{ shaderName, tex, sampler, subresource });
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
		SEAssert("Either the batch or the stage must have a shader", 
			m_stageShader || batch.GetShader());

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