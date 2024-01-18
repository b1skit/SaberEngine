// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "MeshPrimitive.h"
#include "ParameterBlock.h"
#include "PipelineState.h"
#include "ProfilingMarkers.h"
#include "RenderStage.h"
#include "Shader.h"
#include "Texture.h"


namespace
{
	constexpr bool IsBatchAndShaderTopologyCompatible(
		gr::MeshPrimitive::TopologyMode topologyMode, re::PipelineState::TopologyType topologyType)
	{
		// Note: These rules are not complete. If you fail this assert, it's possible you're in a valid state. The goal
		// is to catch unintended accidents
		switch (topologyType)
		{
		case re::PipelineState::TopologyType::Point:
		{
			return topologyMode == gr::MeshPrimitive::TopologyMode::PointList;
		}
		break;
		case re::PipelineState::TopologyType::Line:
		{
			return topologyMode == gr::MeshPrimitive::TopologyMode::LineList ||
				topologyMode == gr::MeshPrimitive::TopologyMode::LineStrip ||
				topologyMode == gr::MeshPrimitive::TopologyMode::LineListAdjacency ||
				topologyMode == gr::MeshPrimitive::TopologyMode::LineStripAdjacency || 
				topologyMode == gr::MeshPrimitive::TopologyMode::TriangleList ||
				topologyMode == gr::MeshPrimitive::TopologyMode::TriangleStrip ||
				topologyMode == gr::MeshPrimitive::TopologyMode::TriangleListAdjacency ||
				topologyMode == gr::MeshPrimitive::TopologyMode::TriangleStripAdjacency;
		}
		break;
		case re::PipelineState::TopologyType::Triangle:
		{
			return topologyMode == gr::MeshPrimitive::TopologyMode::TriangleList ||
				topologyMode == gr::MeshPrimitive::TopologyMode::TriangleStrip ||
				topologyMode == gr::MeshPrimitive::TopologyMode::TriangleListAdjacency ||
				topologyMode == gr::MeshPrimitive::TopologyMode::TriangleStripAdjacency;
		}
		break;
		case re::PipelineState::TopologyType::Patch:
		{
			SEAssertF("Patch topology is (currently) unsupported");
		}
		break;
		default: SEAssertF("Invalid topology type");
		}
		return false;
	}


	void ConfigureClearStage(
		std::shared_ptr<re::RenderStage> newClearStage,
		re::RenderStage::ClearStageParams const& clearStageParams,
		std::shared_ptr<re::TextureTargetSet const> targetSet)
	{
		const uint8_t numColorTargets = targetSet->GetNumColorTargets();

		SEAssert(clearStageParams.m_colorClearModes.size() == 1 ||
			clearStageParams.m_colorClearModes.size() == numColorTargets,
			"Invalid number of color clear modes specified");

		// Create a copy of the targets so we don't modify the originals
		std::shared_ptr<re::TextureTargetSet> clearTargets =
			re::TextureTargetSet::Create(*targetSet, targetSet->GetName() + "_Clear");

		if (numColorTargets > 0)
		{
			if (clearStageParams.m_colorClearModes.size() == 1)
			{
				clearTargets->SetAllColorTargetClearModes(clearStageParams.m_colorClearModes[0]);
			}
			else
			{
				for (size_t targetIdx = 0; targetIdx < numColorTargets; targetIdx++)
				{
					clearTargets->SetColorTargetClearMode(targetIdx, clearStageParams.m_colorClearModes[targetIdx]);
				}
			}
		}
		if (clearTargets->HasDepthTarget())
		{
			clearTargets->SetDepthTargetClearMode(clearStageParams.m_depthClearMode);
		}		

		newClearStage->SetTextureTargetSet(clearTargets);
	}
}


namespace re
{
	std::shared_ptr<RenderStage> RenderStage::CreateParentStage(std::string const& name)
	{
		std::shared_ptr<RenderStage> newParentStage;
		newParentStage.reset(new RenderStage(
			name,
			nullptr,
			RenderStage::Type::Parent,
			RenderStage::Lifetime::Permanent));
		return newParentStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateGraphicsStage(
		std::string const& name, GraphicsStageParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newGFXStage;
		newGFXStage.reset(new RenderStage(
			name, 
			std::make_unique<GraphicsStageParams>(stageParams), 
			RenderStage::Type::Graphics,
			RenderStage::Lifetime::Permanent));
		return newGFXStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateSingleFrameGraphicsStage(
		std::string const& name, GraphicsStageParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newGFXStage;
		newGFXStage.reset(new RenderStage(
			name,
			std::make_unique<GraphicsStageParams>(stageParams),
			RenderStage::Type::Graphics,
			RenderStage::Lifetime::SingleFrame));
		return newGFXStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateComputeStage(
		std::string const& name, ComputeStageParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newComputeStage;
		newComputeStage.reset(new ComputeStage(
			name, 
			std::make_unique<ComputeStageParams>(stageParams), Lifetime::Permanent));
		return newComputeStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateSingleFrameComputeStage(
		std::string const& name, ComputeStageParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newComputeStage;
		newComputeStage.reset(new ComputeStage(
			name,
			std::make_unique<ComputeStageParams>(stageParams), Lifetime::SingleFrame));
		return newComputeStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateClearStage(
		ClearStageParams const& clearStageParams, 
		std::shared_ptr<re::TextureTargetSet const> targetSet)
	{
		std::shared_ptr<RenderStage> newClearStage;
		newClearStage.reset(new ClearStage(targetSet->GetName() + "_Clear", Lifetime::Permanent));

		ConfigureClearStage(newClearStage, clearStageParams, targetSet);

		return newClearStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateSingleFrameClearStage(
		ClearStageParams const& clearStageParams,
		std::shared_ptr<re::TextureTargetSet const> targetSet)
	{
		std::shared_ptr<RenderStage> newClearStage;
		newClearStage.reset(new ClearStage(targetSet->GetName() + "_Clear", Lifetime::SingleFrame));

		ConfigureClearStage(newClearStage, clearStageParams, targetSet);

		return newClearStage;
	}


	RenderStage::RenderStage(
		std::string const& name, 
		std::unique_ptr<IStageParams>&& stageParams, 
		Type stageType, 
		Lifetime lifetime)
		: NamedObject(name)
		, m_type(stageType)
		, m_lifetime(lifetime)
		, m_stageParams(nullptr)
		, m_stageShader(nullptr)
		, m_textureTargetSet(nullptr)
		, m_depthTextureInputIdx(k_noDepthTexAsInputFlag)
		, m_batchFilterBitmask(0)	// Accept all batches by default
	{
		SEAssert(!GetName().empty(), "Invalid RenderStage name");

		m_stageParams = std::move(stageParams);
	}


	ParentStage::ParentStage(
		std::string const& name,
		Lifetime lifetime)
		: NamedObject(name)
		, RenderStage(name, nullptr, Type::Parent, lifetime)
	{
	}


	ComputeStage::ComputeStage(
		std::string const& name, 
		std::unique_ptr<ComputeStageParams>&& stageParams, 
		Lifetime lifetime)
		: NamedObject(name)
		, RenderStage(name, std::move(stageParams), Type::Compute, lifetime)
	{
	}


	ClearStage::ClearStage(
		std::string const& name,
		Lifetime lifetime)
		: NamedObject(name)
		, RenderStage(name, nullptr, Type::Clear, lifetime)
	{
	}


	void RenderStage::SetTextureTargetSet(std::shared_ptr<re::TextureTargetSet> targetSet)
	{
		m_textureTargetSet = targetSet;
		
		m_depthTextureInputIdx = k_noDepthTexAsInputFlag; // Depth target may have changed
		UpdateDepthTextureInputIndex();

		ValidateTexturesAndTargets();
	}


	void RenderStage::AddTextureInput(
		std::string const& shaderName, 
		re::Texture const* tex, 
		std::shared_ptr<re::Sampler> sampler, 
		uint32_t mipLevel /*= re::Texture::k_allMips*/)
	{
		SEAssert(m_stageShader != nullptr, "Stage shader is null. Set the stage shader before this call");
		SEAssert(!shaderName.empty(), "Invalid shader sampler name");
		SEAssert(tex != nullptr, "Invalid texture");
		SEAssert(sampler != nullptr, "Invalid sampler");

		SEAssert((tex->GetTextureParams().m_usage & re::Texture::Usage::Color) != 0,
			"Attempting to add a Texture input that does not have an appropriate usage flag");

		m_textureSamplerInputs.emplace_back(RenderStageTextureAndSamplerInput{ shaderName, tex, sampler, mipLevel });

		UpdateDepthTextureInputIndex();

		ValidateTexturesAndTargets();
	}


	void RenderStage::AddTextureInput(
		std::string const& shaderName,
		std::shared_ptr<re::Texture> tex,
		std::shared_ptr<re::Sampler> sampler,
		uint32_t mipLevel /*= re::Texture::k_allMips*/)
	{
		AddTextureInput(shaderName, tex.get(), sampler, mipLevel);
	}


	void RenderStage::UpdateDepthTextureInputIndex()
	{
		if (m_textureTargetSet == nullptr || m_depthTextureInputIdx != k_noDepthTexAsInputFlag)
		{
			return;
		}

		re::TextureTarget const* depthTarget = m_textureTargetSet->GetDepthStencilTarget();
		if (depthTarget && depthTarget->HasTexture())
		{
			// Check each of our texture inputs against the depth texture:		
			std::shared_ptr<re::Texture> depthTex = depthTarget->GetTexture();
			for (uint32_t i = 0; i < m_textureSamplerInputs.size(); i++)
			{
				if (m_textureSamplerInputs[i].m_texture == depthTex.get())
				{
					m_depthTextureInputIdx = i;

					SEAssert(depthTarget->GetTargetParams().m_channelWriteMode.R == 
							re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled,
						"Depth target has depth writes enabled. It cannot be bound as an input");

					break;
				}
			}
		}
	}


	void RenderStage::ValidateTexturesAndTargets()
	{
		// This is a debug sanity check to make sure we're not trying to bind the same subresources in different ways
#if defined _DEBUG
		if (m_textureTargetSet)
		{
			for (auto const& texInput : m_textureSamplerInputs)
			{
				for (uint8_t i = 0; i < m_textureTargetSet->GetNumColorTargets(); i++)
				{
					SEAssert(m_textureTargetSet->GetColorTarget(i).GetTexture().get() != texInput.m_texture ||
						((m_textureTargetSet->GetColorTarget(i).GetTargetParams().m_targetMip != TextureTarget::k_allFaces &&
							texInput.m_srcMip != TextureTarget::k_allFaces) &&
						m_textureTargetSet->GetColorTarget(i).GetTargetParams().m_targetMip != texInput.m_srcMip),
						"Detected a texture simultaneously used as both a color target and input");
				}

				if (m_textureTargetSet->HasDepthTarget())
				{
					std::shared_ptr<re::Texture const> depthTargetTex = 
						m_textureTargetSet->GetDepthStencilTarget()->GetTexture();

					SEAssert(depthTargetTex.get() != texInput.m_texture ||
						m_textureTargetSet->GetDepthStencilTarget()->GetTargetParams().m_channelWriteMode.R ==
						re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled,
						"A depth target with depth writes enabled cannot also be bound as an input");
				}
			}
		}
#endif
	}


	bool RenderStage::IsSkippable() const
	{
		return (m_stageBatches.empty() && m_type != Type::Clear) ||
			m_type == Type::Parent;
	}
	

	void RenderStage::EndOfFrame()
	{
		SEBeginCPUEvent("StagePipeline::EndOfFrame");

		m_singleFrameParamBlocks.clear();
		m_stageBatches.clear();

		SEEndCPUEvent();
	}


	void RenderStage::AddBatches(std::vector<re::Batch> const& batches)
	{
		SEBeginCPUEvent("RenderStage::AddBatches");

		m_stageBatches.reserve(batches.size());

		for (size_t i = 0; i < batches.size(); i++)
		{
			AddBatch(batches[i]); // Checks filter mask bit before accepting the batch
		}

		SEEndCPUEvent();
	}


	void RenderStage::AddBatch(re::Batch const& batch)
	{
		SEAssert(m_type != re::RenderStage::Type::Parent && 
			m_type != re::RenderStage::Type::Clear,
			"Incompatible stage type");

		SEAssert(m_stageShader || batch.GetShader(), "Either the batch or the stage must have a shader");

		SEAssert((batch.GetType() == re::Batch::BatchType::Graphics && m_type == Type::Graphics) ||
			(batch.GetType() == re::Batch::BatchType::Compute && m_type == Type::Compute),
			"Incompatible batch type");

		SEAssert(m_type == Type::Compute ||
			((!m_stageShader || IsBatchAndShaderTopologyCompatible(
				batch.GetGraphicsParams().m_batchTopologyMode,
				m_stageShader->GetPipelineState().GetTopologyType()) ) &&
			(!batch.GetShader() || IsBatchAndShaderTopologyCompatible(
				batch.GetGraphicsParams().m_batchTopologyMode,
				batch.GetShader()->GetPipelineState().GetTopologyType())) ),
			"Mesh topology mode is incompatible with shader pipeline state topology type");

#if defined(_DEBUG)
		for (auto const& batchPB : batch.GetParameterBlocks())
		{
			for (auto const& singleFramePB : m_singleFrameParamBlocks)
			{
				SEAssert(batchPB->GetUniqueID() != singleFramePB->GetUniqueID(),
					"Batch and render stage have a duplicate single frame parameter block");
			}
			for (auto const& permanentPB : m_permanentParamBlocks)
			{
				SEAssert(batchPB->GetUniqueID() != permanentPB->GetUniqueID(),
					"Batch and render stage have a duplicate permanent parameter block");
			}
		}
#endif
		
		// Filter bits are exclusionary: A RenderStage will not draw a Batch if they have a matching filter bit
		if ((m_batchFilterBitmask & batch.GetBatchFilterMask()) == 0) // Accept all batches by default
		{
			m_stageBatches.emplace_back(batch);
		}
	}


	void RenderStage::SetBatchFilterMaskBit(re::Batch::Filter filterBit)
	{
		m_batchFilterBitmask |= static_cast<uint32_t>(filterBit);
	}


	void RenderStage::AddPermanentParameterBlock(std::shared_ptr<re::ParameterBlock> pb)
	{
		SEAssert(m_lifetime != RenderStage::Lifetime::SingleFrame,
			"SingleFrame RenderStages can only add single frame parameter blocks");
		SEAssert(pb->GetType() == re::ParameterBlock::PBType::Mutable || 
			pb->GetType() == re::ParameterBlock::PBType::Immutable,
			"Parameter block must have a permanent lifetime");
		
		SEAssert(std::find_if(
				m_permanentParamBlocks.begin(),
				m_permanentParamBlocks.end(),
				[&pb](std::shared_ptr<re::ParameterBlock> const& existingPB) {
					return pb->GetNameID() == existingPB->GetNameID();
				}) == m_permanentParamBlocks.end(),
			"A permanent ParameterBlock with this name has already been added");

		SEAssert(std::find_if(
				m_singleFrameParamBlocks.begin(),
				m_singleFrameParamBlocks.end(),
				[&pb](std::shared_ptr<re::ParameterBlock> const& existingPB) {
					return pb->GetNameID() == existingPB->GetNameID();
				}) == m_singleFrameParamBlocks.end(),
			"A single frame ParameterBlock with this name has already been added");

		m_permanentParamBlocks.emplace_back(pb);
	}


	void RenderStage::AddSingleFrameParameterBlock(std::shared_ptr<re::ParameterBlock> pb)
	{
		SEAssert(pb->GetType() == re::ParameterBlock::PBType::SingleFrame,
			"Parameter block must have a single frame lifetime");

		SEAssert(std::find_if(
				m_singleFrameParamBlocks.begin(),
				m_singleFrameParamBlocks.end(),
				[&pb](std::shared_ptr<re::ParameterBlock> const& existingPB) {
					return pb->GetNameID() == existingPB->GetNameID();
				}) == m_singleFrameParamBlocks.end(),
			"A single frame ParameterBlock with this name has already been added");
		
		SEAssert(std::find_if(
				m_permanentParamBlocks.begin(),
				m_permanentParamBlocks.end(),
				[&pb](std::shared_ptr<re::ParameterBlock> const& existingPB) {
					return pb->GetNameID() == existingPB->GetNameID();
				}) == m_permanentParamBlocks.end(),
			"A permanent ParameterBlock with this name has already been added");

		m_singleFrameParamBlocks.emplace_back(pb);
	}
}