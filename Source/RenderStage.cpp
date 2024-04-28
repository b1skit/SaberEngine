// © 2022 Adam Badke. All rights reserved.
#include "Buffer.h"
#include "PipelineState.h"
#include "ProfilingMarkers.h"
#include "RenderStage.h"
#include "RLibrary_Platform.h"
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
			re::TextureTargetSet::Create(*targetSet, targetSet->GetName());

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
	std::shared_ptr<RenderStage> RenderStage::CreateParentStage(char const* name)
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
		char const* name, GraphicsStageParams const& stageParams)
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
		char const* name, GraphicsStageParams const& stageParams)
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
		char const* name, ComputeStageParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newComputeStage;
		newComputeStage.reset(new ComputeStage(
			name, 
			std::make_unique<ComputeStageParams>(stageParams),
			Lifetime::Permanent));
		return newComputeStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateSingleFrameComputeStage(
		char const* name, ComputeStageParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newComputeStage;
		newComputeStage.reset(new ComputeStage(
			name,
			std::make_unique<ComputeStageParams>(stageParams),
			Lifetime::SingleFrame));
		return newComputeStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateLibraryStage(
		char const* name, LibraryStageParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newLibraryStage;
		newLibraryStage.reset(new LibraryStage(
			name,
			std::make_unique<LibraryStageParams>(stageParams),
			Lifetime::Permanent));
		return newLibraryStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateFullscreenQuadStage(
		char const* name, FullscreenQuadParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newFSQuadStage;
		newFSQuadStage.reset(new FullscreenQuadStage(
			name,
			std::make_unique<FullscreenQuadParams>(stageParams),
			Lifetime::Permanent));
		return newFSQuadStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateSingleFrameFullscreenQuadStage(
		char const* name, FullscreenQuadParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newFSQuadStage;
		newFSQuadStage.reset(new FullscreenQuadStage(
			name,
			std::make_unique<FullscreenQuadParams>(stageParams),
			Lifetime::SingleFrame));
		return newFSQuadStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateClearStage(
		ClearStageParams const& clearStageParams, 
		std::shared_ptr<re::TextureTargetSet const> targetSet)
	{
		std::shared_ptr<RenderStage> newClearStage;
		newClearStage.reset(
			new ClearStage(std::format("Clear: {}", targetSet->GetName()).c_str(), Lifetime::Permanent));

		ConfigureClearStage(newClearStage, clearStageParams, targetSet);

		return newClearStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateSingleFrameClearStage(
		ClearStageParams const& clearStageParams,
		std::shared_ptr<re::TextureTargetSet const> targetSet)
	{
		std::shared_ptr<RenderStage> newClearStage;
		newClearStage.reset(
			new ClearStage(std::format("Clear: {}", targetSet->GetName()).c_str(), Lifetime::SingleFrame));

		ConfigureClearStage(newClearStage, clearStageParams, targetSet);

		return newClearStage;
	}


	RenderStage::RenderStage(
		char const* name, std::unique_ptr<IStageParams>&& stageParams, Type stageType, Lifetime lifetime)
		: INamedObject(name)
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


	ParentStage::ParentStage(char const* name, Lifetime lifetime)
		: INamedObject(name)
		, RenderStage(name, nullptr, Type::Parent, lifetime)
	{
	}


	ComputeStage::ComputeStage(char const* name, std::unique_ptr<ComputeStageParams>&& stageParams, Lifetime lifetime)
		: INamedObject(name)
		, RenderStage(name, std::move(stageParams), Type::Compute, lifetime)
	{
	}


	FullscreenQuadStage::FullscreenQuadStage(
		char const* name, std::unique_ptr<FullscreenQuadParams>&& stageParams, Lifetime lifetime)
		: INamedObject(name)
		, RenderStage(name, nullptr, Type::FullscreenQuad, lifetime)
	{
		m_screenAlignedQuad = gr::meshfactory::CreateFullscreenQuad(stageParams->m_zLocation);
		m_fullscreenQuadBatch = std::make_unique<re::Batch>(re::Batch::Lifetime::Permanent, m_screenAlignedQuad.get());
		AddBatch(*m_fullscreenQuadBatch);
	}


	ClearStage::ClearStage(char const* name, Lifetime lifetime)
		: INamedObject(name)
		, RenderStage(name, nullptr, Type::Clear, lifetime)
	{
	}


	void LibraryStage::Execute()
	{
		platform::RLibrary::Execute(this);
	}


	void LibraryStage::SetPayload(std::unique_ptr<IPayload>&& newPayload)
	{
		m_payload = std::move(newPayload);
	}


	std::unique_ptr<LibraryStage::IPayload> LibraryStage::TakePayload()
	{
		return std::move(m_payload);
	}


	LibraryStage::LibraryStage(
		char const* name, std::unique_ptr<LibraryStageParams>&& stageParams, Lifetime lifetime)
		: INamedObject(name)
		, RenderStage(name, std::move(stageParams), Type::Library, lifetime)
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

		bool foundExistingEntry = false;
		if (!m_textureSamplerInputs.empty())
		{
			for (size_t i = 0; i < m_textureSamplerInputs.size(); i++)
			{
				// If we find an input with the same name, replace it:
				if (strcmp(m_textureSamplerInputs[i].m_shaderName.c_str(), shaderName.c_str()) == 0)
				{
					m_textureSamplerInputs[i] = RenderStageTextureAndSamplerInput{ shaderName, tex, sampler, mipLevel };
					foundExistingEntry = true;
					break;
				}
			}
		}
		if (!foundExistingEntry)
		{
			m_textureSamplerInputs.emplace_back(RenderStageTextureAndSamplerInput{ shaderName, tex, sampler, mipLevel });
		}

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

		m_singleFrameBuffers.clear();

		if (m_type != RenderStage::Type::FullscreenQuad) // FSQ stages keep the same batch created during construction
		{
			m_stageBatches.clear();
		}

		SEEndCPUEvent();
	}


	void RenderStage::AddBatches(std::vector<re::Batch> const& batches)
	{
		SEBeginCPUEvent("RenderStage::AddBatches");

		m_stageBatches.reserve(m_stageBatches.size() + batches.size());

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

		SEAssert((m_stageShader || batch.GetShader()) ||
			m_type == RenderStage::Type::FullscreenQuad,
			"Either the batch or the stage must have a shader");

		SEAssert((batch.GetType() == re::Batch::BatchType::Graphics && 
				(m_type == Type::Graphics || m_type == Type::FullscreenQuad)) ||
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

		SEAssert(m_type != Type::FullscreenQuad || m_stageBatches.empty(), 
			"Cannot add batches to a fullscreen quad stage (except for the initial batch during construction)");

#if defined(_DEBUG)
		for (auto const& batchBuffer : batch.GetBuffers())
		{
			for (auto const& singleFrameBuffer : m_singleFrameBuffers)
			{
				SEAssert(batchBuffer->GetUniqueID() != singleFrameBuffer->GetUniqueID(),
					"Batch and render stage have a duplicate single frame buffer");
			}
			for (auto const& permanentBuffer : m_permanentBuffers)
			{
				SEAssert(batchBuffer->GetUniqueID() != permanentBuffer->GetUniqueID(),
					"Batch and render stage have a duplicate permanent buffer");
			}
		}
#endif
		
		// Filter bits are exclusionary: A RenderStage will not draw a Batch if they have a matching filter bit
		if ((m_batchFilterBitmask & batch.GetBatchFilterMask()) == 0) // Accept all batches by default
		{
			m_stageBatches.emplace_back(re::Batch::Duplicate(batch, batch.GetLifetime()));
		}
	}


	void RenderStage::SetBatchFilterMaskBit(re::Batch::Filter filterBit)
	{
		m_batchFilterBitmask |= static_cast<uint32_t>(filterBit);
	}


	void RenderStage::AddPermanentBuffer(std::shared_ptr<re::Buffer> buffer)
	{
		SEAssert(buffer->GetType() == re::Buffer::Type::Mutable || 
			buffer->GetType() == re::Buffer::Type::Immutable,
			"Buffer must have a permanent lifetime");
		
		SEAssert(std::find_if(
				m_permanentBuffers.begin(),
				m_permanentBuffers.end(),
				[&buffer](std::shared_ptr<re::Buffer> const& existingBuffer) {
					return buffer->GetNameID() == existingBuffer->GetNameID();
				}) == m_permanentBuffers.end(),
			"A permanent Buffer with this name has already been added");

		SEAssert(std::find_if(
				m_singleFrameBuffers.begin(),
				m_singleFrameBuffers.end(),
				[&buffer](std::shared_ptr<re::Buffer> const& existingBuffer) {
					return buffer->GetNameID() == existingBuffer->GetNameID();
				}) == m_singleFrameBuffers.end(),
			"A single frame Buffer with this name has already been added");

		m_permanentBuffers.emplace_back(buffer);
	}


	void RenderStage::AddSingleFrameBuffer(std::shared_ptr<re::Buffer> buffer)
	{
		SEAssert(buffer->GetType() == re::Buffer::Type::SingleFrame,
			"Buffer must have a single frame lifetime");

		SEAssert(std::find_if(
				m_singleFrameBuffers.begin(),
				m_singleFrameBuffers.end(),
				[&buffer](std::shared_ptr<re::Buffer> const& existingBuffer) {
					return buffer->GetNameID() == existingBuffer->GetNameID();
				}) == m_singleFrameBuffers.end(),
			"A single frame Buffer with this name has already been added");
		
		SEAssert(std::find_if(
				m_permanentBuffers.begin(),
				m_permanentBuffers.end(),
				[&buffer](std::shared_ptr<re::Buffer> const& existingBuffer) {
					return buffer->GetNameID() == existingBuffer->GetNameID();
				}) == m_permanentBuffers.end(),
			"A permanent Buffer with this name has already been added");

		m_singleFrameBuffers.emplace_back(buffer);
	}
}