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
			RenderStage::RenderStageType::Graphics,
			RenderStage::RenderStageLifetime::Permanent));
		return newGFXStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateComputeStage(
		std::string const& name, ComputeStageParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newComputeStage;
		newComputeStage.reset(new ComputeStage(
			name, 
			std::make_unique<ComputeStageParams>(stageParams), RenderStageLifetime::Permanent));
		return newComputeStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateSingleFrameGraphicsStage(
		std::string const& name, GraphicsStageParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newGFXStage;
		newGFXStage.reset(new RenderStage(
			name,
			std::make_unique<GraphicsStageParams>(stageParams),
			RenderStage::RenderStageType::Graphics,
			RenderStage::RenderStageLifetime::SingleFrame));
		return newGFXStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateSingleFrameComputeStage(
		std::string const& name, ComputeStageParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newComputeStage;
		newComputeStage.reset(new ComputeStage(
			name,
			std::make_unique<ComputeStageParams>(stageParams), RenderStageLifetime::SingleFrame));
		return newComputeStage;
	}


	RenderStage::RenderStage(
		std::string const& name, 
		std::unique_ptr<IStageParams>&& stageParams, 
		RenderStageType stageType, 
		RenderStageLifetime lifetime)
		: NamedObject(name)
		, m_type(stageType)
		, m_lifetime(lifetime)
		, m_stageParams(nullptr)
		, m_stageShader(nullptr)
		, m_textureTargetSet(nullptr)
		, m_batchFilterMask(0)	// Accept all batches by default
	{
		SEAssert("Invalid RenderStage name", !GetName().empty());

		m_stageParams = std::move(stageParams);
	}


	ComputeStage::ComputeStage(
		std::string const& name, 
		std::unique_ptr<ComputeStageParams>&& stageParams, 
		RenderStageLifetime lifetime)
		: NamedObject(name)
		, RenderStage(name, std::move(stageParams), RenderStageType::Compute, lifetime)
	{
	}


	void RenderStage::SetTextureTargetSet(std::shared_ptr<re::TextureTargetSet> targetSet)
	{
		m_textureTargetSet = targetSet;
	}


	void RenderStage::AddTextureInput(
		string const& shaderName, 
		shared_ptr<Texture> tex, 
		shared_ptr<Sampler> sampler, 
		uint32_t mipLevel /*= re::Texture::k_allMips*/)
	{
		SEAssert("Stage shader is null. Set the stage shader before this call", m_stageShader != nullptr);
		SEAssert("Invalid shader sampler name", !shaderName.empty());
		SEAssert("Invalid texture", tex != nullptr);
		SEAssert("Invalid sampler", sampler != nullptr);

		m_textureSamplerInputs.emplace_back(RenderStageTextureAndSamplerInput{ shaderName, tex, sampler, mipLevel });
	}
	

	void RenderStage::EndOfFrame()
	{
		m_singleFrameParamBlocks.clear();
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

		SEAssert("Incompatible batch type", 
			(batch.GetType() == re::Batch::BatchType::Graphics && m_type == RenderStageType::Graphics) ||
			(batch.GetType() == re::Batch::BatchType::Compute && m_type == RenderStageType::Compute));

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
		SEAssert("SingleFrame RenderStages can only add single frame parameter blocks", 
			m_lifetime != RenderStage::RenderStageLifetime::SingleFrame);
		SEAssert("Parameter block must have a permanent lifetime",
			pb->GetType() == re::ParameterBlock::PBType::Mutable || 
			pb->GetType() == re::ParameterBlock::PBType::Immutable);
		
		m_permanentParamBlocks.emplace_back(pb);
	}


	void RenderStage::AddSingleFrameParameterBlock(std::shared_ptr<re::ParameterBlock> pb)
	{
		SEAssert("Parameter block must have a single frame lifetime", 
			pb->GetType() == re::ParameterBlock::PBType::SingleFrame);
		m_singleFrameParamBlocks.emplace_back(pb);
	}
}