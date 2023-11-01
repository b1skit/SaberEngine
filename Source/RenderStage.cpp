// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Batch.h"
#include "MeshPrimitive.h"
#include "PipelineState.h"
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
}


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
		, m_depthTextureInputIdx(k_noDepthTexAsInputFlag)
		, m_batchFilterBitmask(0)	// Accept all batches by default
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
		
		m_depthTextureInputIdx = k_noDepthTexAsInputFlag; // Depth target may have changed
		UpdateDepthTextureInputIndex();

		ValidateTexturesAndTargets();
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

		SEAssert("Attempting to add a Texture input that does not have an appropriate usage flag",
			(tex->GetTextureParams().m_usage & re::Texture::Usage::Color));

		m_textureSamplerInputs.emplace_back(RenderStageTextureAndSamplerInput{ shaderName, tex, sampler, mipLevel });

		UpdateDepthTextureInputIndex();

		ValidateTexturesAndTargets();
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
				if (m_textureSamplerInputs[i].m_texture == depthTex)
				{
					m_depthTextureInputIdx = i;

					SEAssert("Depth target has depth writes enabled. It cannot be bound as an input", 
						depthTarget->GetTargetParams().m_channelWriteMode.R == 
							re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled);

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
					SEAssert("Detected a texture simultaneously used as both a color target and input",
						m_textureTargetSet->GetColorTarget(i).GetTexture() != texInput.m_texture ||
						((m_textureTargetSet->GetColorTarget(i).GetTargetParams().m_targetMip != TextureTarget::k_allFaces &&
							texInput.m_srcMip != TextureTarget::k_allFaces) &&
						m_textureTargetSet->GetColorTarget(i).GetTargetParams().m_targetMip != texInput.m_srcMip));
				}

				if (m_textureTargetSet->HasDepthTarget())
				{
					std::shared_ptr<re::Texture const> depthTargetTex = 
						m_textureTargetSet->GetDepthStencilTarget()->GetTexture();

					SEAssert("A depth target with depth writes enabled cannot also be bound as an input",
						depthTargetTex != texInput.m_texture ||
						m_textureTargetSet->GetDepthStencilTarget()->GetTargetParams().m_channelWriteMode.R ==
						re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled);
				}
			}
		}
#endif
	}
	

	void RenderStage::EndOfFrame()
	{
		m_singleFrameParamBlocks.clear();
		m_stageBatches.clear();
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
		SEAssert("Either the batch or the stage must have a shader", m_stageShader || batch.GetShader());

		SEAssert("Incompatible batch type", 
			(batch.GetType() == re::Batch::BatchType::Graphics && m_type == RenderStageType::Graphics) ||
			(batch.GetType() == re::Batch::BatchType::Compute && m_type == RenderStageType::Compute));

		SEAssert("Mesh topology mode is incompatible with shader pipeline state topology type",
			m_type == RenderStageType::Compute ||
			((!m_stageShader || IsBatchAndShaderTopologyCompatible(
				batch.GetGraphicsParams().m_batchTopologyMode,
				m_stageShader->GetPipelineState().GetTopologyType()) ) &&
			(!batch.GetShader() || IsBatchAndShaderTopologyCompatible(
				batch.GetGraphicsParams().m_batchTopologyMode,
				batch.GetShader()->GetPipelineState().GetTopologyType())) ));
		
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