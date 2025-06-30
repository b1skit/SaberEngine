// © 2022 Adam Badke. All rights reserved.
#include "BatchBuilder.h"
#include "BatchFactories.h"
#include "Buffer.h"
#include "BufferView.h"
#include "EffectDB.h"
#include "IndexedBuffer.h"
#include "RenderManager.h"
#include "RenderObjectIDs.h"
#include "RLibrary_Platform.h"
#include "Stage.h"
#include "Sampler.h"
#include "SwapChain.h"
#include "SwapChain_Platform.h"
#include "Texture.h"

#include "Core/ProfilingMarkers.h"

#include "Core/Util/HashKey.h"

#include "Renderer/Shaders/Common/InstancingParams.h"


namespace gr
{
	std::shared_ptr<Stage> Stage::CreateParentStage(char const* name)
	{
		std::shared_ptr<Stage> newParentStage;
		newParentStage.reset(new Stage(
			name,
			nullptr,
			Stage::Type::Parent,
			re::Lifetime::Permanent));
		return newParentStage;
	}


	std::shared_ptr<Stage> Stage::CreateGraphicsStage(
		char const* name, GraphicsStageParams const& stageParams)
	{
		std::shared_ptr<Stage> newGFXStage;
		newGFXStage.reset(new Stage(
			name, 
			std::make_unique<GraphicsStageParams>(stageParams), 
			Stage::Type::Raster,
			re::Lifetime::Permanent));

		newGFXStage->m_instancingEnabled = true; // Instancing is enabled by default for raster stages

		return newGFXStage;
	}


	std::shared_ptr<Stage> Stage::CreateSingleFrameGraphicsStage(
		char const* name, GraphicsStageParams const& stageParams)
	{
		std::shared_ptr<Stage> newGFXStage;
		newGFXStage.reset(new Stage(
			name,
			std::make_unique<GraphicsStageParams>(stageParams),
			Stage::Type::Raster,
			re::Lifetime::SingleFrame));
		
		newGFXStage->m_instancingEnabled = true; // Instancing is enabled by default for raster stages

		return newGFXStage;
	}


	std::shared_ptr<Stage> Stage::CreateComputeStage(
		char const* name, ComputeStageParams const& stageParams)
	{
		std::shared_ptr<Stage> newComputeStage;
		newComputeStage.reset(new ComputeStage(
			name, 
			std::make_unique<ComputeStageParams>(stageParams),
			re::Lifetime::Permanent));
		return newComputeStage;
	}


	std::shared_ptr<Stage> Stage::CreateSingleFrameComputeStage(
		char const* name, ComputeStageParams const& stageParams)
	{
		std::shared_ptr<Stage> newComputeStage;
		newComputeStage.reset(new ComputeStage(
			name,
			std::make_unique<ComputeStageParams>(stageParams),
			re::Lifetime::SingleFrame));
		return newComputeStage;
	}


	std::shared_ptr<Stage> Stage::CreateLibraryStage(
		char const* name, LibraryStageParams const& stageParams)
	{
		SEAssert(IsLibraryType(stageParams.m_stageType), "Library stages must specify a Library stage type");

		std::shared_ptr<Stage> newLibraryStage;
		newLibraryStage.reset(new LibraryStage(
			name,
			std::make_unique<LibraryStageParams>(stageParams),
			re::Lifetime::Permanent));

		if (newLibraryStage->m_type == Stage::Type::LibraryRaster)
		{
			newLibraryStage->m_instancingEnabled = true; // Instancing is enabled by default for raster stages
		}

		return newLibraryStage;
	}


	std::shared_ptr<Stage> Stage::CreateFullscreenQuadStage(
		char const* name, FullscreenQuadParams const& stageParams)
	{
		std::shared_ptr<Stage> newFSQuadStage;
		newFSQuadStage.reset(new FullscreenQuadStage(
			name,
			std::make_unique<FullscreenQuadParams>(stageParams),
			re::Lifetime::Permanent));
		return newFSQuadStage;
	}


	std::shared_ptr<Stage> Stage::CreateSingleFrameFullscreenQuadStage(
		char const* name, FullscreenQuadParams const& stageParams)
	{
		std::shared_ptr<Stage> newFSQuadStage;
		newFSQuadStage.reset(new FullscreenQuadStage(
			name,
			std::make_unique<FullscreenQuadParams>(stageParams),
			re::Lifetime::SingleFrame));
		return newFSQuadStage;
	}


	std::shared_ptr<Stage> Stage::CreateRayTracingStage(
		char const* name, RayTracingStageParams const& stageParams)
	{
		std::shared_ptr<Stage> newRTStage;
		newRTStage.reset(new RayTracingStage(
			name,
			std::make_unique<RayTracingStageParams>(stageParams),
			re::Lifetime::Permanent));
		return newRTStage;
	}


	std::shared_ptr<Stage> Stage::CreateSingleFrameRayTracingStage(
		char const* name, RayTracingStageParams const& stageParams)
	{
		std::shared_ptr<Stage> newRTStage;
		newRTStage.reset(new RayTracingStage(
			name,
			std::make_unique<RayTracingStageParams>(stageParams),
			re::Lifetime::SingleFrame));
		return newRTStage;
	}


	std::shared_ptr<ClearTargetSetStage> Stage::CreateTargetSetClearStage(
		char const* name,
		std::shared_ptr<re::TextureTargetSet> const& targetSet)
	{
		std::shared_ptr<ClearTargetSetStage> newClearStage;

		newClearStage.reset(new ClearTargetSetStage(
			std::format("Clear Stage: {} ({})", name, targetSet->GetName()).c_str(),
			re::Lifetime::Permanent));

		newClearStage->SetTextureTargetSet(targetSet);

		return newClearStage;
	}


	std::shared_ptr<ClearTargetSetStage> Stage::CreateSingleFrameTargetSetClearStage(
		char const* name,
		std::shared_ptr<re::TextureTargetSet> const& targetSet)
	{
		std::shared_ptr<ClearTargetSetStage> newClearStage;
		newClearStage.reset(new ClearTargetSetStage(
			std::format("Clear Stage: {} ({})", name, targetSet->GetName()).c_str(),
			re::Lifetime::SingleFrame));

		newClearStage->SetTextureTargetSet(targetSet);

		return newClearStage;
	}


	std::shared_ptr<ClearRWTexturesStage> Stage::CreateRWTextureClearStage(char const* name)
	{
		std::shared_ptr<ClearRWTexturesStage> newClearStage;
		newClearStage.reset(new ClearRWTexturesStage(name, re::Lifetime::Permanent));
		return newClearStage;
	}


	std::shared_ptr<ClearRWTexturesStage> Stage::CreateSingleFrameRWTextureClearStage(char const* name)
	{
		std::shared_ptr<ClearRWTexturesStage> newClearStage;
		newClearStage.reset(new ClearRWTexturesStage(name, re::Lifetime::SingleFrame));
		return newClearStage;
	}


	std::shared_ptr<CopyStage> Stage::CreateCopyStage(
		core::InvPtr<re::Texture> const& src,
		core::InvPtr<re::Texture> const& dst)
	{
		SEAssert(src.IsValid(), "Copy source must be valid");
		std::string const& stageName = 
			std::format("Copy Stage: {} to {}", src->GetName(), dst.IsValid() ? dst->GetName().c_str() : "Backbuffer");
		
		std::shared_ptr<CopyStage> newCopyStage;
		newCopyStage.reset(new CopyStage(stageName.c_str(), re::Lifetime::Permanent, src, dst));

		return newCopyStage;
	}


	std::shared_ptr<CopyStage> Stage::CreateSingleFrameCopyStage(
		core::InvPtr<re::Texture> const& src,
		core::InvPtr<re::Texture> const& dst)
	{
		std::string const& stageName =
			std::format("Copy Stage: {} to {}", src->GetName(), dst ? dst->GetName().c_str() : "Backbuffer");

		std::shared_ptr<CopyStage> newCopyStage;
		newCopyStage.reset(new CopyStage(stageName.c_str(), re::Lifetime::SingleFrame, src, dst));

		return newCopyStage;
	}


	Stage::Stage(
		char const* name, std::unique_ptr<IStageParams>&& stageParams, Type stageType, re::Lifetime lifetime)
		: INamedObject(name)
		, m_type(stageType)
		, m_lifetime(lifetime)
		, m_stageParams(nullptr)
		, m_drawStyleBits(0)
		, m_textureTargetSet(nullptr)
		, m_depthTextureInputIdx(k_noDepthTexAsInputFlag)
		, m_requiredBatchFilterBitmasks(0)	// Accept all batches by default
		, m_excludedBatchFilterBitmasks(0)
		, m_instancingEnabled(false)
	{
		SEAssert(!GetName().empty(), "Invalid Stage name");

		m_stageParams = std::move(stageParams);
	}


	ParentStage::ParentStage(char const* name, re::Lifetime lifetime)
		: INamedObject(name)
		, Stage(name, nullptr, Type::Parent, lifetime)
	{
	}


	ComputeStage::ComputeStage(char const* name, std::unique_ptr<ComputeStageParams>&& stageParams, re::Lifetime lifetime)
		: INamedObject(name)
		, Stage(name, std::move(stageParams), Type::Compute, lifetime)
	{
	}


	FullscreenQuadStage::FullscreenQuadStage(
		char const* name, std::unique_ptr<FullscreenQuadParams>&& stageParams, re::Lifetime lifetime)
		: INamedObject(name)
		, Stage(name, nullptr, Type::FullscreenQuad, lifetime)
	{
		SEAssert(stageParams->m_effectID != 0, "Invalid EffectID");

		m_screenAlignedQuad = 
			gr::meshfactory::CreateFullscreenQuad(re::RenderManager::Get()->GetInventory(), stageParams->m_zLocation);

		m_drawStyleBits = stageParams->m_drawStyleBitmask;

		m_fullscreenQuadBatch = gr::RasterBatchBuilder::CreateMeshPrimitiveBatch(
			m_screenAlignedQuad,
			stageParams->m_effectID,
			grutil::BuildMeshPrimitiveRasterBatch)
				.Build();
		
		AddBatch(m_fullscreenQuadBatch);
	}


	RayTracingStage::RayTracingStage(
		char const* name, std::unique_ptr<RayTracingStageParams>&& stageParams, re::Lifetime lifetime)
		: INamedObject(name)
		, Stage(name, std::move(stageParams), Type::RayTracing, lifetime)
	{
	}


	ClearTargetSetStage::ClearTargetSetStage(char const* name, re::Lifetime lifetime)
		: INamedObject(name)
		, Stage(name, nullptr, Type::ClearTargetSet, lifetime)
		, m_colorClearModes(nullptr)
		, m_colorClearValues(nullptr)
		, m_numColorClears(0)
		, m_depthClearVal(1.f) // Far plane
		, m_stencilClearVal(0)
		, m_depthClearMode(false)
		, m_stencilClearMode(false)
	{
	}


	ClearRWTexturesStage::ClearRWTexturesStage(char const* name, re::Lifetime lifetime)
		: INamedObject(name)
		, Stage(name, nullptr, Type::ClearRWTextures, lifetime)
		, m_clearValueType(ClearRWTexturesStage::ValueType::Float)
		, m_clearFloat(0.f)
	{
	}


	void ClearRWTexturesStage::AddPermanentRWTextureInput(
		core::InvPtr<re::Texture> const& tex, re::TextureView const& texView)
	{
		SEAssert(tex != nullptr, "Invalid texture");
		SEAssert((tex->GetTextureParams().m_usage & re::Texture::ColorTarget) != 0,
			"Invalid usage");

#if defined(_DEBUG)
		for (auto const& singleFrameRWTexInput : m_singleFrameRWTextureInputs)
		{
			SEAssert(tex->GetName() != singleFrameRWTexInput.m_texture->GetName(),
				"A texture input with the texture same name has already been added a single frame RW input. This may "
				"be valid if the TextureView is different, but we need to implement TextureView comparisons");
		}
#endif

		bool foundExistingEntry = false;
		for (size_t i = 0; i < m_permanentRWTextureInputs.size(); ++i)
		{
			// If we find an input with the same name, replace it:
			if (m_permanentRWTextureInputs[i].m_texture->GetNameHash() == tex->GetNameHash())
			{
				m_permanentRWTextureInputs[i] = re::RWTextureInput{ k_dummyShaderName, tex, texView };
				foundExistingEntry = true;
				break;
			}
		}
		if (!foundExistingEntry)
		{
			m_permanentRWTextureInputs.emplace_back(re::RWTextureInput{ k_dummyShaderName, tex, texView });
		}
	}


	void ClearRWTexturesStage::AddSingleFrameRWTextureInput(
		core::InvPtr<re::Texture> const& tex, re::TextureView const& texView)
	{
		SEAssert(tex != nullptr, "Invalid texture");
		SEAssert((tex->GetTextureParams().m_usage & re::Texture::ColorTarget) != 0,
			"Invalid usage");

#if defined(_DEBUG)
		for (auto const& permanentRWTexInput : m_permanentRWTextureInputs)
		{
			SEAssert(permanentRWTexInput.m_texture->GetNameHash() != tex->GetNameHash(),
				"A texture input with the same name has already been added a permanent input");
		}
		for (auto const& singleFrameRWTexInput : m_singleFrameRWTextureInputs)
		{
			SEAssert(singleFrameRWTexInput.m_texture->GetNameHash() != tex->GetNameHash(),
				"A RW texture input with the same name has already been added a single frame input. Re-adding the same "
				"single frame texture is not allowed");
		}
#endif
		m_singleFrameRWTextureInputs.emplace_back(k_dummyShaderName, tex, texView);
	}


	CopyStage::CopyStage(
		char const* name,
		re::Lifetime lifetime,
		core::InvPtr<re::Texture> const& src,
		core::InvPtr<re::Texture> const& dst)
		: INamedObject(name)
		, Stage(name, nullptr, Type::Copy, lifetime)
		, m_src(src)
		, m_dst(dst)
	{
		SEAssert(m_src, "Invalid copy stage source");
		SEAssert(m_src != m_dst, "Can only copy different resources");

#if defined(_DEBUG)
		// Validate the copy complies with D3D12 restrictions (OpenGL is far more permissive) 
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copyresource
		if (dst.IsValid())
		{
			SEAssert(src != dst, "Copy source and destination must be different resources");

			SEAssert(m_src->GetTotalBytesPerFace() == dst->GetTotalBytesPerFace(),
				"Can only copy textures of the same size");

			SEAssert(m_src->Width() == dst->Width() &&
				m_src->Height() == dst->Height() &&
				re::Texture::GetNumFaces(m_src) == re::Texture::GetNumFaces(dst) &&
				m_src->GetNumMips() == dst->GetNumMips(),
				"Can only copy textures with identical dimensions");

			SEAssert(re::Texture::IsCompatibleGroupFormat(
				m_src->GetTextureParams().m_format, dst->GetTextureParams().m_format),
				"Formats must be identical or from the same type group");

			SEAssert((m_src->GetTextureParams().m_usage & re::Texture::Usage::ColorSrc) &&
				((dst->GetTextureParams().m_usage & re::Texture::Usage::SwapchainColorProxy) ||
					((dst->GetTextureParams().m_usage & re::Texture::Usage::ColorTarget) ||
					((dst->GetTextureParams().m_usage & re::Texture::Usage::DepthTarget) && 
						m_src->GetTextureParams().m_usage & re::Texture::Usage::DepthTarget))),
				"Source/destination texture flags are incorrect");
		}
		else
		{
			re::SwapChain const& swapchain = re::RenderManager::Get()->GetContext()->GetSwapChain();
			glm::uvec2 const& swapchainDims = platform::SwapChain::GetBackbufferDimensions(swapchain);

			SEAssert(m_src->Width() == swapchainDims.x && m_src->Height() == swapchainDims.y,
				"Can only copy to the backbuffer from textures with identical dimensions");

			SEAssert(re::Texture::IsCompatibleGroupFormat(
				m_src->GetTextureParams().m_format, platform::SwapChain::GetBackbufferFormat(swapchain)),
				"Formats must be identical or from the same type group");

			SEAssert((m_src->GetTextureParams().m_usage & re::Texture::Usage::ColorSrc),
				"Source texture flags are incorrect");
		}
#endif
	}


	void LibraryStage::Execute(void* platformObject)
	{
		platform::RLibrary::Execute(this, platformObject);
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
		char const* name, std::unique_ptr<LibraryStageParams>&& stageParams, re::Lifetime lifetime)
		: INamedObject(name)
		, Stage(name, std::move(stageParams), stageParams->m_stageType, lifetime)
	{
	}


	void Stage::SetTextureTargetSet(std::shared_ptr<re::TextureTargetSet> const& targetSet)
	{
		m_textureTargetSet = targetSet;

		m_depthTextureInputIdx = k_noDepthTexAsInputFlag; // Depth target may have changed
	}


	void Stage::AddPermanentTextureInput(
		char const* shaderName,
		core::InvPtr<re::Texture> const& tex,
		core::InvPtr<re::Sampler> const& sampler,
		re::TextureView const& texView)
	{
		SEAssert(shaderName && strlen(shaderName), "Invalid texture name");
		SEAssert(tex != nullptr, "Invalid texture");
		SEAssert(sampler.IsValid(), "Invalid sampler");

		SEAssert((tex->GetTextureParams().m_usage & re::Texture::Usage::ColorSrc) != 0,
			"Attempting to add a Texture input that does not have an appropriate usage flag");

#if defined(_DEBUG)
		for (auto const& singleFrameTexInput : m_singleFrameTextureSamplerInputs)
		{
			SEAssert(singleFrameTexInput.m_shaderName != shaderName,
				"A texture input with the same name has already been added a single frame input");
		}
#endif

		bool foundExistingEntry = false;
		if (!m_permanentTextureSamplerInputs.empty())
		{
			for (size_t i = 0; i < m_permanentTextureSamplerInputs.size(); i++)
			{
				// If we find an input with the same name, replace it:
				if (strcmp(m_permanentTextureSamplerInputs[i].m_shaderName.c_str(), shaderName) == 0)
				{
					m_permanentTextureSamplerInputs[i] = re::TextureAndSamplerInput(shaderName, tex, sampler, texView);
					foundExistingEntry = true;
					break;
				}
			}
		}
		if (!foundExistingEntry)
		{
			m_permanentTextureSamplerInputs.emplace_back(re::TextureAndSamplerInput{shaderName, tex, sampler, texView});
		}

		if (m_textureTargetSet && 
			m_textureTargetSet->HasDepthTarget() &&
			tex == m_textureTargetSet->GetDepthStencilTarget().GetTexture())
		{
			m_depthTextureInputIdx = k_noDepthTexAsInputFlag; // Need to revalidate
		}
	}


	void Stage::AddPermanentTextureInput(
		std::string const& shaderName,
		core::InvPtr<re::Texture> const& tex,
		core::InvPtr<re::Sampler> const& sampler,
		re::TextureView const& texView)
	{
		SEAssert(!shaderName.empty(), "Invalid shader sampler name");
		AddPermanentTextureInput(shaderName.c_str(), tex, sampler, texView);
	}


	void Stage::AddSingleFrameTextureInput(
		char const* shaderName,
		core::InvPtr<re::Texture> const& tex,
		core::InvPtr<re::Sampler> const& sampler,
		re::TextureView const& texView)
	{
		SEAssert(shaderName, "Shader name cannot be null");
		SEAssert(tex.IsValid(), "Invalid texture");
		SEAssert(sampler != nullptr, "Invalid sampler");

		SEAssert((tex->GetTextureParams().m_usage & re::Texture::Usage::ColorSrc) != 0,
			"Attempting to add a Texture input that does not have an appropriate usage flag");

#if defined(_DEBUG)
		for (auto const& permanentTexInput : m_permanentTextureSamplerInputs)
		{
			SEAssert(strcmp(permanentTexInput.m_shaderName.c_str(), shaderName) != 0,
				"A texture input with the same name has already been added a permanent input");
		}
		for (auto const& singleFrameTexInput : m_singleFrameTextureSamplerInputs)
		{
			SEAssert(strcmp(singleFrameTexInput.m_shaderName.c_str(), shaderName) != 0,
				"A texture input with the same name has already been added a single frame input. Re-adding the same "
				"single frame texture is not allowed");
		}
#endif
		m_singleFrameTextureSamplerInputs.emplace_back(shaderName, tex, sampler, texView);


		if (m_textureTargetSet &&
			m_textureTargetSet->HasDepthTarget() &&
			tex == m_textureTargetSet->GetDepthStencilTarget().GetTexture())
		{
			m_depthTextureInputIdx = k_noDepthTexAsInputFlag; // Need to revalidate
		}
	}


	void Stage::AddSingleFrameTextureInput(
		std::string const& shaderName,
		core::InvPtr<re::Texture> const& tex,
		core::InvPtr<re::Sampler> const& sampler,
		re::TextureView const& texView)
	{
		AddSingleFrameTextureInput(shaderName.c_str(), tex, sampler, texView);
	}


	void Stage::UpdateDepthTextureInputIndex()
	{
		SEBeginCPUEvent("Stage::UpdateDepthTextureInputIndex");

		if (m_textureTargetSet == nullptr || m_depthTextureInputIdx != k_noDepthTexAsInputFlag)
		{
			SEEndCPUEvent(); // "Stage::UpdateDepthTextureInputIndex"
			return;
		}

		re::TextureTarget const& depthTarget = m_textureTargetSet->GetDepthStencilTarget();
		if (depthTarget.HasTexture())
		{
			const bool depthTargetWritesEnabled = depthTarget.GetTargetParams().m_textureView.DepthWritesEnabled();

			// Check each of our texture inputs against the depth texture:		
			core::InvPtr<re::Texture> const& depthTex = depthTarget.GetTexture();

			for (uint32_t i = 0; i < m_permanentTextureSamplerInputs.size(); i++)
			{
				if (m_permanentTextureSamplerInputs[i].m_texture == depthTex)
				{
					m_depthTextureInputIdx = i;

					SEAssert(!depthTargetWritesEnabled,
						"Depth target has depth writes enabled. It cannot be bound as an input");

					break;
				}
			}
		}

		SEEndCPUEvent(); // "Stage::UpdateDepthTextureInputIndex"
	}


	void Stage::ResolveBatches(gr::IndexedBufferManager& ibm, effect::EffectDB const& effectDB)
	{
		SEBeginCPUEvent("Stage::ResolveBatches");

		// Early out:
		if (m_resolvedBatches.empty() || m_instancingEnabled == false)
		{
			// Resolve the batches without trying to apply instancing
			for (auto& stageBatchHandle : m_resolvedBatches)
			{
				stageBatchHandle.Resolve(m_drawStyleBits, 1, effectDB);
			}

			SEEndCPUEvent(); // "Stage::ResolveBatches"
			return;
		}

		struct BatchMetadata
		{
			gr::BatchHandle const* m_batchHandle;
			gr::StageBatchHandle const* m_stageBatchHandle;
		};

		// Populate the batch metadata:
		SEBeginCPUEvent("Populate batchMetadata");

		std::vector<BatchMetadata> batchMetadata;
		batchMetadata.reserve(m_resolvedBatches.size());
		for (size_t i = 0; i < m_resolvedBatches.size(); i++)
		{
			batchMetadata.emplace_back( 
				&(*m_resolvedBatches[i]),
				&m_resolvedBatches[i]);
		}

		SEEndCPUEvent(); // "Populate batchMetadata"


		// Sort the batch metadata:
		SEBeginCPUEvent("Sort batchMetadata");

		std::sort(batchMetadata.begin(), batchMetadata.end(),
			[](BatchMetadata const& a, BatchMetadata const& b) 
			{ return (*a.m_batchHandle)->GetDataHash() < (*b.m_batchHandle)->GetDataHash(); });
			
		SEEndCPUEvent(); // "Sort batchMetadata"


		// Merge the batches:
		SEBeginCPUEvent("Merge batches");

		std::vector<gr::StageBatchHandle> mergedBatches;
		mergedBatches.reserve(m_resolvedBatches.size()); // Over-estimation
		
		size_t unmergedIdx = 0;
		do
		{
			gr::StageBatchHandle const& stageBatchHandle = *batchMetadata[unmergedIdx].m_stageBatchHandle;

			// Add the first batch in the sequence to our final list. We duplicate the batch, as cached batches
			// have a permanent Lifetime
			mergedBatches.emplace_back(stageBatchHandle);

			// Find the index of the last batch with a matching hash in the sequence:
			const uint64_t curBatchHash = (*batchMetadata[unmergedIdx].m_batchHandle)->GetDataHash();
			const size_t instanceStartIdx = unmergedIdx++;
			while (unmergedIdx < batchMetadata.size() &&
				(*batchMetadata[unmergedIdx].m_batchHandle)->GetDataHash() == curBatchHash)
			{
				unmergedIdx++;
			}

			// Compute and set the number of instances in the batch:
			const uint32_t numInstances = util::CheckedCast<uint32_t, size_t>(unmergedIdx - instanceStartIdx);

			// Resolve the batch: Internally, this gets the Shader, sets the instance count, and resolves raster
			// batch vertex streams etc 
			mergedBatches.back().Resolve(m_drawStyleBits, numInstances, effectDB);

			// Attach the instance and LUT buffers:
			effect::Effect const* batchEffect = effectDB.GetEffect((*mergedBatches.back())->GetEffectID());

			auto const& effectBufferShaderNames = batchEffect->GetRequestedBufferShaderNames();

			bool setInstanceBuffer = false;
			if (!effectBufferShaderNames.empty())
			{
				for (auto const& bufferNameHash : effectBufferShaderNames)
				{
					mergedBatches.back().SetSingleFrameBuffer(
						ibm.GetIndexedBufferInput(bufferNameHash.first, bufferNameHash.second.c_str()));

					setInstanceBuffer = true;
				}
			}
			
			// Indexed buffer LUTs require a valid RenderDataID, but it's still valid to attach an instanced buffer
			// (e.g. if the GS handled the LUT manually)
			if (setInstanceBuffer &&
				(*stageBatchHandle).GetRenderDataID() != gr::k_invalidRenderDataID)
			{
				// Use a view of our batch metadata to get the list of RenderDataIDs for each instance:
				std::ranges::range auto&& instancedBatchView = batchMetadata
					| std::views::drop(instanceStartIdx)
					| std::views::take(numInstances)
					| std::ranges::views::transform([](BatchMetadata const& batchMetadata) -> gr::RenderDataID
						{
							return batchMetadata.m_batchHandle->GetRenderDataID();
						});

				mergedBatches.back().SetSingleFrameBuffer(
					ibm.GetLUTBufferInput<InstanceIndexData>(InstanceIndexData::s_shaderName, instancedBatchView));
			}

		} while (unmergedIdx < batchMetadata.size());

		// Swap in our merged results:
		m_resolvedBatches = std::move(mergedBatches);

		SEEndCPUEvent(); // "Merge batches"

		SEEndCPUEvent(); // "Stage::ResolveBatches"
	}


	void Stage::AddPermanentRWTextureInput(
		std::string const& shaderName,
		core::InvPtr<re::Texture> const& tex,
		re::TextureView const& texView)
	{
		SEAssert(!shaderName.empty(), "Invalid shader sampler name");
		SEAssert(tex != nullptr, "Invalid texture");
		SEAssert((tex->GetTextureParams().m_usage & re::Texture::ColorSrc) != 0 &&
			(tex->GetTextureParams().m_usage & re::Texture::ColorTarget) != 0,
			"Invalid usage");

#if defined(_DEBUG)
		for (auto const& singleFrameRWTexInput : m_singleFrameRWTextureInputs)
		{
			SEAssert(singleFrameRWTexInput.m_shaderName != shaderName,
				"A texture input with the same name has already been added a single frame RW input");
		}
#endif

		bool foundExistingEntry = false;
		if (!m_permanentRWTextureInputs.empty())
		{
			for (size_t i = 0; i < m_permanentRWTextureInputs.size(); i++)
			{
				// If we find an input with the same name, replace it:
				if (strcmp(m_permanentRWTextureInputs[i].m_shaderName.c_str(), shaderName.c_str()) == 0)
				{
					m_permanentRWTextureInputs[i] = re::RWTextureInput{ shaderName, tex, texView };
					foundExistingEntry = true;
					break;
				}
			}
		}
		if (!foundExistingEntry)
		{
			m_permanentRWTextureInputs.emplace_back(re::RWTextureInput{ shaderName, tex, texView });
		}

		if (m_textureTargetSet &&
			m_textureTargetSet->HasDepthTarget() &&
			tex == m_textureTargetSet->GetDepthStencilTarget().GetTexture())
		{
			m_depthTextureInputIdx = k_noDepthTexAsInputFlag; // Need to revalidate
		}
	}


	void Stage::AddSingleFrameRWTextureInput(
		char const* shaderName,
		core::InvPtr<re::Texture> const& tex,
		re::TextureView const& texView)
	{
		SEAssert(shaderName, "Shader name cannot be null");
		SEAssert(tex != nullptr, "Invalid texture");
		SEAssert((tex->GetTextureParams().m_usage & re::Texture::ColorSrc) != 0 &&
			(tex->GetTextureParams().m_usage & re::Texture::ColorTarget) != 0,
			"Invalid usage");

#if defined(_DEBUG)
		for (auto const& permanentRWTexInput : m_permanentRWTextureInputs)
		{
			SEAssert(strcmp(permanentRWTexInput.m_shaderName.c_str(), shaderName) != 0,
				"A texture input with the same name has already been added a permanent input");
		}
		for (auto const& singleFrameRWTexInput : m_singleFrameRWTextureInputs)
		{
			SEAssert(strcmp(singleFrameRWTexInput.m_shaderName.c_str(), shaderName) != 0,
				"A RW texture input with the same name has already been added a single frame input. Re-adding the same "
				"single frame texture is not allowed");
		}
#endif
		m_singleFrameRWTextureInputs.emplace_back(shaderName, tex, texView);


		if (m_textureTargetSet &&
			m_textureTargetSet->HasDepthTarget() &&
			tex == m_textureTargetSet->GetDepthStencilTarget().GetTexture())
		{
			m_depthTextureInputIdx = k_noDepthTexAsInputFlag; // Need to revalidate
		}
	}


	void Stage::ValidateTexturesAndTargets()
	{
		// This is a debug sanity check to make sure we're not trying to bind the same subresources in different ways
#if defined _DEBUG
		if (m_textureTargetSet)
		{
			auto ValidateInput = [this](auto const& texSamplerInputs)
				{
					for (auto const& texInput : texSamplerInputs)
					{
						for (uint8_t i = 0; i < m_textureTargetSet->GetNumColorTargets(); i++)
						{
							core::InvPtr<re::Texture> const& targetTex = m_textureTargetSet->GetColorTarget(i).GetTexture();
							re::TextureView const& targetTexView = 
								m_textureTargetSet->GetColorTarget(i).GetTargetParams().m_textureView;

							core::InvPtr<re::Texture> const& inputTex = texInput.m_texture;
							re::TextureView const& inputTexView = texInput.m_textureView;

							if (targetTex != inputTex)
							{
								continue;
							}

							SEAssert(inputTexView.m_viewDimension == targetTexView.m_viewDimension,
								"Using the same texture as an input and target, but with different dimensions. This is "
								"not (currently) supported (it would require updating this validator)");

							uint32_t inputFirstMip = std::numeric_limits<uint32_t>::max();
							uint32_t inputMipLevels = std::numeric_limits<uint32_t>::max();
							uint32_t inputFirstArraySlice = std::numeric_limits<uint32_t>::max();
							uint32_t inputArraySize = std::numeric_limits<uint32_t>::max();

							uint32_t targetFirstMip = std::numeric_limits<uint32_t>::max();
							uint32_t targetMipLevels = std::numeric_limits<uint32_t>::max();
							uint32_t targetFirstArraySlice = std::numeric_limits<uint32_t>::max();
							uint32_t targetArraySize = std::numeric_limits<uint32_t>::max();							

							bool isArrayType = false;

							switch (targetTexView.m_viewDimension)
							{
							case re::Texture::Dimension::Texture1D:
							{
								inputFirstMip = inputTexView.Texture1D.m_firstMip;
								inputMipLevels = inputTexView.Texture1D.m_mipLevels;

								targetFirstMip = targetTexView.Texture1D.m_firstMip;
								targetMipLevels = targetTexView.Texture1D.m_mipLevels;
							}
							break;
							case re::Texture::Dimension::Texture1DArray:
							{
								inputFirstMip = inputTexView.Texture1DArray.m_firstMip;
								inputMipLevels = inputTexView.Texture1DArray.m_mipLevels;
								inputFirstArraySlice = inputTexView.Texture1DArray.m_firstArraySlice;
								inputArraySize = inputTexView.Texture1DArray.m_arraySize;

								targetFirstMip = targetTexView.Texture1DArray.m_firstMip;
								targetMipLevels = targetTexView.Texture1DArray.m_mipLevels;
								targetFirstArraySlice = targetTexView.Texture1DArray.m_firstArraySlice;
								targetArraySize = inputTexView.Texture1DArray.m_arraySize;

								isArrayType = true;
							}
							break;
							case re::Texture::Dimension::Texture2D:
							{
								inputFirstMip = inputTexView.Texture2D.m_firstMip;
								inputMipLevels = inputTexView.Texture2D.m_mipLevels;

								targetFirstMip = targetTexView.Texture2D.m_firstMip;
								targetMipLevels = targetTexView.Texture2D.m_mipLevels;
							}
							break;
							case re::Texture::Dimension::Texture2DArray:
							{
								inputFirstMip = inputTexView.Texture2DArray.m_firstMip;
								inputMipLevels = inputTexView.Texture2DArray.m_mipLevels;
								inputFirstArraySlice = inputTexView.Texture2DArray.m_firstArraySlice;
								inputArraySize = inputTexView.Texture2DArray.m_arraySize;

								targetFirstMip = targetTexView.Texture2DArray.m_firstMip;
								targetMipLevels = targetTexView.Texture2DArray.m_mipLevels;
								targetFirstArraySlice = targetTexView.Texture2DArray.m_firstArraySlice;
								targetArraySize = inputTexView.Texture2DArray.m_arraySize;

								isArrayType = true;
							}
							break;
							case re::Texture::Dimension::Texture3D:
							{
								inputFirstMip = inputTexView.Texture3D.m_firstMip;
								inputMipLevels = inputTexView.Texture3D.m_mipLevels;

								targetFirstMip = targetTexView.Texture3D.m_firstMip;
								targetMipLevels = targetTexView.Texture3D.m_mipLevels;
							}
							break;
							case re::Texture::Dimension::TextureCube:
							{
								inputFirstMip = inputTexView.TextureCube.m_firstMip;
								inputMipLevels = inputTexView.TextureCube.m_mipLevels;

								targetFirstMip = targetTexView.TextureCube.m_firstMip;
								targetMipLevels = targetTexView.TextureCube.m_mipLevels;
							}
							break;
							case re::Texture::Dimension::TextureCubeArray:
							{
								inputFirstMip = inputTexView.TextureCubeArray.m_firstMip;
								inputMipLevels = inputTexView.TextureCubeArray.m_mipLevels;
								inputFirstArraySlice = inputTexView.TextureCubeArray.m_first2DArrayFace;
								inputArraySize = inputTexView.TextureCubeArray.m_numCubes * 6;

								targetFirstMip = targetTexView.TextureCubeArray.m_firstMip;
								targetMipLevels = targetTexView.TextureCubeArray.m_mipLevels;
								targetFirstArraySlice = targetTexView.TextureCubeArray.m_first2DArrayFace;
								targetArraySize = targetTexView.TextureCubeArray.m_numCubes * 6;

								isArrayType = true;
							}
							break;
							default: SEAssertF("Invalid dimension");
							}

							SEAssert(inputMipLevels != re::Texture::k_allMips &&
								targetTexView.Texture1D.m_mipLevels != re::Texture::k_allMips,
								"Cannot view all mips on a texture used as both an input and target");

							if (isArrayType)
							{
								const uint32_t numInputMips = inputTex->GetNumMips();
								const uint32_t numTargetMips = targetTex->GetNumMips();

								const uint32_t firstInputSubresouce =
									(inputFirstArraySlice + inputArraySize) * numInputMips + inputFirstMip;

								const uint32_t lastInputSubresource = 
									(inputFirstArraySlice + inputArraySize) * numInputMips + inputFirstMip + inputMipLevels;

								const uint32_t firstTargetSubresouce = 
									(targetFirstArraySlice + targetArraySize) * numTargetMips + targetFirstMip;

								const uint32_t lastTargetSubresource =
									(targetFirstArraySlice + targetArraySize) * numTargetMips + targetFirstMip + targetMipLevels;


								SEAssert(lastInputSubresource <= firstTargetSubresouce ||
									lastTargetSubresource <= firstInputSubresouce,
									"View is overlapping subresources");
							}
							else
							{
								SEAssert(inputFirstMip + inputMipLevels <= targetFirstMip ||
									targetFirstMip + targetMipLevels <= inputFirstMip,
									"View is overlapping subresources");
							}
						}

						if (m_textureTargetSet->HasDepthTarget())
						{
							re::TextureTarget const& depthTarget = m_textureTargetSet->GetDepthStencilTarget();
							core::InvPtr<re::Texture> const& depthTargetTex = depthTarget.GetTexture();
							
							SEAssert(depthTargetTex != texInput.m_texture ||
								!depthTarget.GetTargetParams().m_textureView.DepthWritesEnabled(),
								std::format("The Stage \"{}\" is trying to use the depth target \"{} \" as both "
									"an input, and a target. Depth targets with depth writes enabled cannot also be "
									"bound as an input. "
									"NOTE: This assert doesn't consider non-overlapping mip indexes, but it should!",
									GetName(),
									depthTargetTex->GetName()).c_str());
						}
					}
				};

			ValidateInput(m_permanentTextureSamplerInputs);
			ValidateInput(m_singleFrameTextureSamplerInputs);

			ValidateInput(m_permanentRWTextureInputs);
			ValidateInput(m_singleFrameRWTextureInputs);

			for (auto const& batch : m_resolvedBatches)
			{
				ValidateInput((*batch)->GetTextureAndSamplerInputs());
				ValidateInput((*batch)->GetRWTextureInputs());
			}

			// Validate depth texture usage
			re::TextureTarget const* depthTarget = &m_textureTargetSet->GetDepthStencilTarget();
			if (depthTarget && depthTarget->HasTexture())
			{
				core::InvPtr<re::Texture> const& depthTex = depthTarget->GetTexture();

				for (auto const& singleFrameInput : m_singleFrameTextureSamplerInputs)
				{
					SEAssert(singleFrameInput.m_texture != depthTex,
						"Setting the depth texture as a single frame input is not (currently) supported (DEPRECATED?)");
				}

				for (auto const& singleFrameRWInput : m_singleFrameRWTextureInputs)
				{
					SEAssert(singleFrameRWInput.m_texture != depthTex,
						"Setting the depth texture as a single frame RW input is not (currently) supported (DEPRECATED?)");
				}
			}
		}
#endif
	}


	bool Stage::IsSkippable() const
	{
		if (m_type == Type::ClearTargetSet || 
			m_type == Type::ClearRWTextures ||
			IsLibraryType(m_type) || 
			m_type == Type::Copy)
		{
			return false; // Assume library and utility stages always do work
		}
		return m_type == Type::Parent || m_resolvedBatches.empty();

		SEStaticAssert(static_cast<uint8_t>(gr::Stage::Type::Invalid) == 10,
			"Number of stage types has changed. This must be updated");
	}
	

	void Stage::PostUpdatePreRender(gr::IndexedBufferManager& ibm, effect::EffectDB const& effectDB)
	{
		SEBeginCPUEvent("Stage::PostUpdatePreRender");

		UpdateDepthTextureInputIndex();

		ResolveBatches(ibm, effectDB);

		ValidateTexturesAndTargets(); // _DEBUG only

		SEEndCPUEvent(); // "Stage::PostUpdatePreRender"
	}


	void Stage::EndOfFrame()
	{
		SEBeginCPUEvent("StagePipeline::EndOfFrame");

		m_singleFrameBuffers.clear();
		m_singleFrameTextureSamplerInputs.clear();
		m_singleFrameRWTextureInputs.clear();

		if (m_type != Stage::Type::FullscreenQuad) // FSQ stages keep the same batch created during construction
		{
			m_resolvedBatches.clear();
		}

		SEEndCPUEvent();
	}


	void Stage::AddBatches(std::vector<gr::BatchHandle> const& batches)
	{
		SEBeginCPUEvent("Stage::AddBatches");

		m_resolvedBatches.reserve(m_resolvedBatches.size() + batches.size());

		for (size_t i = 0; i < batches.size(); i++)
		{
			AddBatch(batches[i]); // Checks filter mask bit before accepting the batch
		}

		SEEndCPUEvent();
	}


	gr::StageBatchHandle* Stage::AddBatch(gr::BatchHandle batch)
	{
		SEAssert(m_type != gr::Stage::Type::Parent &&
			m_type != gr::Stage::Type::ClearTargetSet,
			"Incompatible stage type: Cannot add batches");

		SEAssert(m_type != Type::FullscreenQuad || m_resolvedBatches.empty(),
			"Cannot add batches to a fullscreen quad stage (except for the initial batch during construction)");

		SEAssert(batch->GetEffectID() != 0 ||
			batch->GetType() == re::Batch::BatchType::RayTracing,
			"Batch has not been assigned an Effect");

		SEAssert((batch->GetType() == re::Batch::BatchType::Raster &&
			(m_type == Type::Raster || m_type == Type::FullscreenQuad)) ||
			(batch->GetType() == re::Batch::BatchType::Compute && m_type == Type::Compute) ||
			(batch->GetType() == re::Batch::BatchType::RayTracing && m_type == Type::RayTracing),
			"Incompatible batch type");

#if defined(_DEBUG)
		for (auto const& batchBufferInput : batch->GetBuffers())
		{
			for (auto const& singleFrameBufferInput : m_singleFrameBuffers)
			{
				SEAssert(batchBufferInput.GetBuffer()->GetUniqueID() != singleFrameBufferInput.GetBuffer()->GetUniqueID() &&
					batchBufferInput.GetShaderNameHash() != singleFrameBufferInput.GetShaderNameHash(),
					"Batch and render stage have a duplicate single frame buffer");
			}
			for (auto const& permanentBuffer : m_permanentBuffers)
			{
				SEAssert(batchBufferInput.GetBuffer()->GetUniqueID() != permanentBuffer.GetBuffer()->GetUniqueID() &&
					batchBufferInput.GetShaderNameHash() != permanentBuffer.GetShaderNameHash(),
					"Batch and render stage have a duplicate permanent buffer");
			}
		}
#endif

		if (batch->MatchesFilterBits(m_requiredBatchFilterBitmasks, m_excludedBatchFilterBitmasks))
		{
			gr::StageBatchHandle& unresolvedBatch = m_resolvedBatches.emplace_back(batch);
			return &unresolvedBatch;
		}
		return nullptr;
	}


	void Stage::SetBatchFilterMaskBit(re::Batch::Filter filterBit, FilterMode mode, bool enabled)
	{
		switch (mode)
		{
		case FilterMode::Require:
		{
			if (enabled)
			{
				m_requiredBatchFilterBitmasks |= static_cast<re::Batch::FilterBitmask>(filterBit);
				if (m_excludedBatchFilterBitmasks & static_cast<re::Batch::FilterBitmask>(filterBit))
				{
					m_excludedBatchFilterBitmasks ^= static_cast<re::Batch::FilterBitmask>(filterBit);
				}
			}
			else if (m_requiredBatchFilterBitmasks & static_cast<re::Batch::FilterBitmask>(filterBit))
			{
				m_requiredBatchFilterBitmasks ^= (1 << static_cast<re::Batch::FilterBitmask>(filterBit));
			}
		}
		break;
		case FilterMode::Exclude:
		{
			if (enabled)
			{
				m_excludedBatchFilterBitmasks |= static_cast<re::Batch::FilterBitmask>(filterBit);
				if (m_requiredBatchFilterBitmasks & static_cast<re::Batch::FilterBitmask>(filterBit))
				{
					m_requiredBatchFilterBitmasks ^= static_cast<re::Batch::FilterBitmask>(filterBit);
				}
			}
			else if (m_excludedBatchFilterBitmasks & static_cast<re::Batch::FilterBitmask>(filterBit))
			{
				m_excludedBatchFilterBitmasks ^= static_cast<re::Batch::FilterBitmask>(filterBit);
			}
		}
		break;
		default: SEAssertF("Invalid filter bit mode");
		}
	}


	void Stage::AddPermanentBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const& buffer)
	{
		AddPermanentBuffer(re::BufferInput(shaderName, buffer));
	}


	void Stage::AddPermanentBuffer(
		std::string const& shaderName, std::shared_ptr<re::Buffer> const& buffer, re::BufferView const& view)
	{
		AddPermanentBuffer(re::BufferInput(shaderName, buffer, view));
	}


	void Stage::AddPermanentBuffer(re::BufferInput&& bufferInput)
	{
		SEAssert(bufferInput.GetLifetime() == re::Lifetime::Permanent, "Invalid BufferInput lifetime");
		SEAssert(bufferInput.GetBuffer()->GetLifetime() == re::Lifetime::Permanent, "Invalid Buffer lifetime");
		SEAssert(!bufferInput.GetShaderName().empty() && bufferInput.GetBuffer(), "Buffer cannot be unnamed or null");

		SEAssert(std::find_if(
			m_permanentBuffers.begin(),
			m_permanentBuffers.end(),
			[&bufferInput](re::BufferInput const& existingBuffer)
			{
				const bool matchingNameHash = bufferInput.GetShaderNameHash() == existingBuffer.GetShaderNameHash();
				if (matchingNameHash)
				{
					// Duplicate names are allowed if we're binding to a Constrant/Structured buffer array
					if ((re::Buffer::HasUsageBit(re::Buffer::Usage::Constant, *bufferInput.GetBuffer()) &&
							re::Buffer::HasUsageBit(re::Buffer::Usage::Constant, *existingBuffer.GetBuffer())) ||
						(re::Buffer::HasUsageBit(re::Buffer::Usage::Structured, *bufferInput.GetBuffer()) &&
							re::Buffer::HasUsageBit(re::Buffer::Usage::Structured, *existingBuffer.GetBuffer())))
					{
						return bufferInput.GetView().m_bufferView.m_firstDestIdx ==
							existingBuffer.GetView().m_bufferView.m_firstDestIdx;
					}
				}
				return false;
			}) == m_permanentBuffers.end(),
				"A permanent Buffer with this shader name has already been added");

		SEAssert(std::find_if(
			m_singleFrameBuffers.begin(),
			m_singleFrameBuffers.end(),
			[&bufferInput](re::BufferInput const& existingBuffer)
			{
				const bool matchingNameHash = bufferInput.GetShaderNameHash() == existingBuffer.GetShaderNameHash();
				if (matchingNameHash)
				{
					// Duplicate names are allowed if we're binding to a Constrant/Structured buffer array
					if ((re::Buffer::HasUsageBit(re::Buffer::Usage::Constant, *bufferInput.GetBuffer()) &&
							re::Buffer::HasUsageBit(re::Buffer::Usage::Constant, *existingBuffer.GetBuffer())) ||
						(re::Buffer::HasUsageBit(re::Buffer::Usage::Structured, *bufferInput.GetBuffer()) &&
							re::Buffer::HasUsageBit(re::Buffer::Usage::Structured, *existingBuffer.GetBuffer())))
					{
						return bufferInput.GetView().m_bufferView.m_firstDestIdx ==
							existingBuffer.GetView().m_bufferView.m_firstDestIdx;
					}
				}
				return false;
			}) == m_singleFrameBuffers.end(),
				"A single frame Buffer with this shader name has already been added");

		m_permanentBuffers.emplace_back(std::move(bufferInput));
	}


	void Stage::AddPermanentBuffer(re::BufferInput const& bufferInput)
	{
		AddPermanentBuffer(re::BufferInput(bufferInput));
	}


	void Stage::AddSingleFrameBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const& buffer)
	{
		AddSingleFrameBuffer(re::BufferInput(shaderName, buffer));
	}


	void Stage::AddSingleFrameBuffer(
		std::string const& shaderName, std::shared_ptr<re::Buffer> const& buffer, re::BufferView const& view)
	{
		AddSingleFrameBuffer(re::BufferInput(shaderName, buffer, view));
	}


	void Stage::AddSingleFrameBuffer(re::BufferInput&& bufferInput)
	{
		SEAssert(!bufferInput.GetShaderName().empty() && bufferInput.GetBuffer(), "Buffer cannot be unnamed or null");

		SEAssert(std::find_if(
			m_singleFrameBuffers.begin(),
			m_singleFrameBuffers.end(),
			[&bufferInput](re::BufferInput const& existingBuffer) {
				return bufferInput.GetShaderNameHash() == existingBuffer.GetShaderNameHash();
			}) == m_singleFrameBuffers.end(),
				"A single frame Buffer with shader name \"%s\" has already been added",
				bufferInput.GetShaderName().c_str());

		SEAssert(std::find_if(
			m_permanentBuffers.begin(),
			m_permanentBuffers.end(),
			[&bufferInput](re::BufferInput const& existingBuffer) {
				return bufferInput.GetShaderNameHash() == existingBuffer.GetShaderNameHash();
			}) == m_permanentBuffers.end(),
				"A permanent Buffer with shader name \"%s\" has already been added",
				bufferInput.GetShaderName().c_str());

		m_singleFrameBuffers.emplace_back(std::move(bufferInput));
	}


	void Stage::AddSingleFrameBuffer(re::BufferInput const& bufferInput)
	{
		AddSingleFrameBuffer(re::BufferInput(bufferInput));
	}
}