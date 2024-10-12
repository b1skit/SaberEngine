// © 2022 Adam Badke. All rights reserved.
#include "Buffer.h"
#include "BufferView.h"
#include "RenderStage.h"
#include "RLibrary_Platform.h"
#include "Shader.h"
#include "Texture.h"

#include "Core/ProfilingMarkers.h"


namespace
{
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
			re::Lifetime::Permanent));
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
			re::Lifetime::Permanent));
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
			re::Lifetime::SingleFrame));
		return newGFXStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateComputeStage(
		char const* name, ComputeStageParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newComputeStage;
		newComputeStage.reset(new ComputeStage(
			name, 
			std::make_unique<ComputeStageParams>(stageParams),
			re::Lifetime::Permanent));
		return newComputeStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateSingleFrameComputeStage(
		char const* name, ComputeStageParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newComputeStage;
		newComputeStage.reset(new ComputeStage(
			name,
			std::make_unique<ComputeStageParams>(stageParams),
			re::Lifetime::SingleFrame));
		return newComputeStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateLibraryStage(
		char const* name, LibraryStageParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newLibraryStage;
		newLibraryStage.reset(new LibraryStage(
			name,
			std::make_unique<LibraryStageParams>(stageParams),
			re::Lifetime::Permanent));
		return newLibraryStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateFullscreenQuadStage(
		char const* name, FullscreenQuadParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newFSQuadStage;
		newFSQuadStage.reset(new FullscreenQuadStage(
			name,
			std::make_unique<FullscreenQuadParams>(stageParams),
			re::Lifetime::Permanent));
		return newFSQuadStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateSingleFrameFullscreenQuadStage(
		char const* name, FullscreenQuadParams const& stageParams)
	{
		std::shared_ptr<RenderStage> newFSQuadStage;
		newFSQuadStage.reset(new FullscreenQuadStage(
			name,
			std::make_unique<FullscreenQuadParams>(stageParams),
			re::Lifetime::SingleFrame));
		return newFSQuadStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateClearStage(
		ClearStageParams const& clearStageParams, 
		std::shared_ptr<re::TextureTargetSet const> targetSet)
	{
		std::shared_ptr<RenderStage> newClearStage;
		newClearStage.reset(
			new ClearStage(std::format("Clear: {}", targetSet->GetName()).c_str(), re::Lifetime::Permanent));

		ConfigureClearStage(newClearStage, clearStageParams, targetSet);

		return newClearStage;
	}


	std::shared_ptr<RenderStage> RenderStage::CreateSingleFrameClearStage(
		ClearStageParams const& clearStageParams,
		std::shared_ptr<re::TextureTargetSet const> targetSet)
	{
		std::shared_ptr<RenderStage> newClearStage;
		newClearStage.reset(
			new ClearStage(std::format("Clear: {}", targetSet->GetName()).c_str(), re::Lifetime::SingleFrame));

		ConfigureClearStage(newClearStage, clearStageParams, targetSet);

		return newClearStage;
	}


	RenderStage::RenderStage(
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
	{
		SEAssert(!GetName().empty(), "Invalid RenderStage name");

		m_stageParams = std::move(stageParams);
	}


	ParentStage::ParentStage(char const* name, re::Lifetime lifetime)
		: INamedObject(name)
		, RenderStage(name, nullptr, Type::Parent, lifetime)
	{
	}


	ComputeStage::ComputeStage(char const* name, std::unique_ptr<ComputeStageParams>&& stageParams, re::Lifetime lifetime)
		: INamedObject(name)
		, RenderStage(name, std::move(stageParams), Type::Compute, lifetime)
	{
	}


	FullscreenQuadStage::FullscreenQuadStage(
		char const* name, std::unique_ptr<FullscreenQuadParams>&& stageParams, re::Lifetime lifetime)
		: INamedObject(name)
		, RenderStage(name, nullptr, Type::FullscreenQuad, lifetime)
	{
		SEAssert(stageParams->m_effectID.IsValid(), "Invalid EffectID");

		m_screenAlignedQuad = gr::meshfactory::CreateFullscreenQuad(stageParams->m_zLocation);

		m_drawStyleBits = stageParams->m_drawStyleBitmask;

		m_fullscreenQuadBatch = std::make_unique<re::Batch>(
			re::Lifetime::Permanent,
			m_screenAlignedQuad.get(),
			stageParams->m_effectID);
		
		AddBatch(*m_fullscreenQuadBatch);
	}


	ClearStage::ClearStage(char const* name, re::Lifetime lifetime)
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
		char const* name, std::unique_ptr<LibraryStageParams>&& stageParams, re::Lifetime lifetime)
		: INamedObject(name)
		, RenderStage(name, std::move(stageParams), Type::Library, lifetime)
	{
	}


	void RenderStage::SetTextureTargetSet(std::shared_ptr<re::TextureTargetSet> targetSet)
	{
		m_textureTargetSet = targetSet;

		m_depthTextureInputIdx = k_noDepthTexAsInputFlag; // Depth target may have changed
	}


	void RenderStage::AddPermanentTextureInput(
		std::string const& shaderName,
		re::Texture const* tex,
		re::Sampler const* sampler,
		re::TextureView const& texView)
	{
		SEAssert(!shaderName.empty(), "Invalid shader sampler name");
		SEAssert(tex != nullptr, "Invalid texture");
		SEAssert(sampler != nullptr, "Invalid sampler");

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
				if (strcmp(m_permanentTextureSamplerInputs[i].m_shaderName.c_str(), shaderName.c_str()) == 0)
				{
					m_permanentTextureSamplerInputs[i] = re::TextureAndSamplerInput{shaderName, tex, sampler, texView};
					foundExistingEntry = true;
					break;
				}
			}
		}
		if (!foundExistingEntry)
		{
			m_permanentTextureSamplerInputs.emplace_back(TextureAndSamplerInput{shaderName, tex, sampler, texView});
		}

		if (m_textureTargetSet && 
			m_textureTargetSet->HasDepthTarget() &&
			tex == m_textureTargetSet->GetDepthStencilTarget().GetTexture().get())
		{
			m_depthTextureInputIdx = k_noDepthTexAsInputFlag; // Need to revalidate
		}
	}


	void RenderStage::AddPermanentTextureInput(
		std::string const& shaderName,
		std::shared_ptr<re::Texture> tex,
		std::shared_ptr<re::Sampler> sampler,
		re::TextureView const& texView)
	{
		AddPermanentTextureInput(shaderName, tex.get(), sampler.get(), texView);
	}


	void RenderStage::AddSingleFrameTextureInput(
		char const* shaderName,
		re::Texture const* tex,
		std::shared_ptr<re::Sampler> sampler,
		re::TextureView const& texView)
	{
		SEAssert(shaderName, "Shader name cannot be null");
		SEAssert(tex != nullptr, "Invalid texture");
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
		m_singleFrameTextureSamplerInputs.emplace_back(shaderName, tex, sampler.get(), texView);


		if (m_textureTargetSet &&
			m_textureTargetSet->HasDepthTarget() &&
			tex == m_textureTargetSet->GetDepthStencilTarget().GetTexture().get())
		{
			m_depthTextureInputIdx = k_noDepthTexAsInputFlag; // Need to revalidate
		}
	}


	void RenderStage::AddSingleFrameTextureInput(
		char const* shaderName,
		std::shared_ptr<re::Texture> tex,
		std::shared_ptr<re::Sampler> sampler,
		re::TextureView const& texView)
	{
		AddSingleFrameTextureInput(shaderName, tex.get(), sampler, texView);
	}


	void RenderStage::UpdateDepthTextureInputIndex()
	{
		if (m_textureTargetSet == nullptr || m_depthTextureInputIdx != k_noDepthTexAsInputFlag)
		{
			return;
		}

		re::TextureTarget const& depthTarget = m_textureTargetSet->GetDepthStencilTarget();
		if (depthTarget.HasTexture())
		{
			const bool depthTargetWritesEnabled = depthTarget.GetTargetParams().m_textureView.DepthWritesEnabled();

			// Check each of our texture inputs against the depth texture:		
			re::Texture const* depthTex = depthTarget.GetTexture().get();

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
	}


	void RenderStage::AddPermanentRWTextureInput(
		std::string const& shaderName,
		re::Texture const* tex,
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
			tex == m_textureTargetSet->GetDepthStencilTarget().GetTexture().get())
		{
			m_depthTextureInputIdx = k_noDepthTexAsInputFlag; // Need to revalidate
		}
	}


	void RenderStage::AddPermanentRWTextureInput(
		std::string const& shaderName,
		std::shared_ptr<re::Texture> tex,
		re::TextureView const& texView)
	{
		AddPermanentRWTextureInput(shaderName, tex.get(), texView);
	}


	void RenderStage::AddSingleFrameRWTextureInput(
		char const* shaderName,
		re::Texture const* tex,
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
			tex == m_textureTargetSet->GetDepthStencilTarget().GetTexture().get())
		{
			m_depthTextureInputIdx = k_noDepthTexAsInputFlag; // Need to revalidate
		}
	}


	void RenderStage::AddSingleFrameRWTextureInput(
		char const* shaderName,
		std::shared_ptr<re::Texture> tex,
		re::TextureView const& texView)
	{
		AddSingleFrameRWTextureInput(shaderName, tex.get(), texView);
	}


	void RenderStage::ValidateTexturesAndTargets()
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
							re::Texture const* targetTex = m_textureTargetSet->GetColorTarget(i).GetTexture().get();
							re::TextureView const& targetTexView = 
								m_textureTargetSet->GetColorTarget(i).GetTargetParams().m_textureView;

							re::Texture const* inputTex = texInput.m_texture;
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
							re::Texture const* depthTargetTex = depthTarget.GetTexture().get();
							
							SEAssert(depthTargetTex != texInput.m_texture ||
								!depthTarget.GetTargetParams().m_textureView.DepthWritesEnabled(),
								std::format("The RenderStage \"{}\" is trying to use the depth target \"{} \" as both "
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

			for (auto const& batch : m_stageBatches)
			{
				ValidateInput(batch.GetTextureAndSamplerInputs());
				ValidateInput(batch.GetRWTextureInputs());
			}

			// Validate depth texture usage
			re::TextureTarget const* depthTarget = &m_textureTargetSet->GetDepthStencilTarget();
			if (depthTarget && depthTarget->HasTexture())
			{
				re::Texture const* depthTex = depthTarget->GetTexture().get();

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


	bool RenderStage::IsSkippable() const
	{
		return (m_stageBatches.empty() && m_type != Type::Clear) ||
			m_type == Type::Parent;
	}
	

	void RenderStage::PostUpdatePreRender()
	{
		UpdateDepthTextureInputIndex();
		ValidateTexturesAndTargets(); // _DEBUG only
	}


	void RenderStage::EndOfFrame()
	{
		SEBeginCPUEvent("StagePipeline::EndOfFrame");

		m_singleFrameBuffers.clear();
		m_singleFrameTextureSamplerInputs.clear();
		m_singleFrameRWTextureInputs.clear();

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


	re::Batch* RenderStage::AddBatch(re::Batch const& batch)
	{
		return AddBatchWithLifetime(batch, batch.GetLifetime());
	}


	re::Batch* RenderStage::AddBatchWithLifetime(re::Batch const& batch, re::Lifetime lifetime)
	{
		SEAssert(m_type != re::RenderStage::Type::Parent &&
			m_type != re::RenderStage::Type::Clear,
			"Incompatible stage type: Cannot add batches");

		SEAssert(m_type != Type::FullscreenQuad || m_stageBatches.empty(),
			"Cannot add batches to a fullscreen quad stage (except for the initial batch during construction)");

		SEAssert(batch.GetEffectID().IsValid(), "Batch has not been assigned an Effect");

		SEAssert((batch.GetType() == re::Batch::BatchType::Graphics &&
			(m_type == Type::Graphics || m_type == Type::FullscreenQuad)) ||
			(batch.GetType() == re::Batch::BatchType::Compute && m_type == Type::Compute),
			"Incompatible batch type");

#if defined(_DEBUG)
		for (auto const& batchBufferInput : batch.GetBuffers())
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

		if (batch.MatchesFilterBits(m_requiredBatchFilterBitmasks, m_excludedBatchFilterBitmasks))
		{
			re::Batch* duplicatedBatch = &m_stageBatches.emplace_back(re::Batch::Duplicate(batch, lifetime));
			duplicatedBatch->ResolveShader(m_drawStyleBits);

			return duplicatedBatch;
		}
		return nullptr;
	}


	void RenderStage::SetBatchFilterMaskBit(re::Batch::Filter filterBit, FilterMode mode, bool enabled)
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


	void RenderStage::AddPermanentBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const& buffer)
	{
		AddPermanentBuffer(re::BufferInput(shaderName, buffer));
	}


	void RenderStage::AddPermanentBuffer(
		std::string const& shaderName, std::shared_ptr<re::Buffer> const& buffer, re::BufferView const& view)
	{
		AddPermanentBuffer(re::BufferInput(shaderName, buffer, view));
	}


	void RenderStage::AddPermanentBuffer(re::BufferInput const& bufferInput)
	{
		AddPermanentBuffer(re::BufferInput(bufferInput));
	}


	void RenderStage::AddPermanentBuffer(re::BufferInput&& bufferInput)
	{
		SEAssert(!bufferInput.GetShaderName().empty() && bufferInput.GetBuffer(), "Buffer cannot be unnamed or null");

		SEAssert(bufferInput.GetBuffer()->GetLifetime() == re::Lifetime::Permanent,
			"Buffer must have a permanent lifetime");

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
						return bufferInput.GetView().m_buffer.m_firstDestIdx ==
							existingBuffer.GetView().m_buffer.m_firstDestIdx;
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
						return bufferInput.GetView().m_buffer.m_firstDestIdx ==
							existingBuffer.GetView().m_buffer.m_firstDestIdx;
					}
				}
				return false;
			}) == m_singleFrameBuffers.end(),
				"A single frame Buffer with this shader name has already been added");

		m_permanentBuffers.emplace_back(std::move(bufferInput));
	}


	void RenderStage::AddSingleFrameBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> const& buffer)
	{
		AddSingleFrameBuffer(re::BufferInput(shaderName, buffer));
	}


	void RenderStage::AddSingleFrameBuffer(
		std::string const& shaderName, std::shared_ptr<re::Buffer> const& buffer, re::BufferView const& view)
	{
		AddSingleFrameBuffer(re::BufferInput(shaderName, buffer, view));
	}


	void RenderStage::AddSingleFrameBuffer(re::BufferInput const& bufferInput)
	{
		AddSingleFrameBuffer(re::BufferInput(bufferInput));
	}


	void RenderStage::AddSingleFrameBuffer(re::BufferInput&& bufferInput)
	{
		SEAssert(!bufferInput.GetShaderName().empty() && bufferInput.GetBuffer(), "Buffer cannot be unnamed or null");

		SEAssert(std::find_if(
			m_singleFrameBuffers.begin(),
			m_singleFrameBuffers.end(),
			[&bufferInput](re::BufferInput const& existingBuffer) {
				return bufferInput.GetShaderNameHash() == existingBuffer.GetShaderNameHash();
			}) == m_singleFrameBuffers.end(),
				"A single frame Buffer with this shader name has already been added");

		SEAssert(std::find_if(
			m_permanentBuffers.begin(),
			m_permanentBuffers.end(),
			[&bufferInput](re::BufferInput const& existingBuffer) {
				return bufferInput.GetShaderNameHash() == existingBuffer.GetShaderNameHash();
			}) == m_permanentBuffers.end(),
				"A permanent Buffer with this shader name has already been added");

		m_singleFrameBuffers.emplace_back(std::move(bufferInput));
	}
}