// © 2023 Adam Badke. All rights reserved.
#include "GraphicsSystem_ComputeMips.h"
#include "RenderManager.h"
#include "Sampler.h"

#include "Core/Definitions/ConfigKeys.h"

#include "Core/Util/MathUtils.h"

#include "Shaders/Common/MipGenerationParams.h"


namespace
{
	MipGenerationData CreateMipGenerationParamsData(
		std::shared_ptr<re::Texture> tex, uint32_t srcMipLevel, uint32_t numMips, uint32_t faceIdx, uint32_t arrayIdx)
	{
		re::Texture::TextureParams const& texParams = tex->GetTextureParams();

		const uint32_t output0MipLevel = srcMipLevel + 1;
		glm::vec4 const& output0Dimensions = tex->GetMipLevelDimensions(output0MipLevel);

		/* Calculate the odd/even flag:
		#define SRC_WIDTH_EVEN_HEIGHT_EVEN 0
		#define SRC_WIDTH_ODD_HEIGHT_EVEN 1
		#define SRC_WIDTH_EVEN_HEIGHT_ODD 2
		#define SRC_WIDTH_ODD_HEIGHT_ODD 3 */
		glm::vec4 const& srcDimensions = tex->GetMipLevelDimensions(srcMipLevel);

		uint32_t srcDimensionMode = (static_cast<uint32_t>(srcDimensions.x) % 2); // 1 if x is odd
		srcDimensionMode |= ((static_cast<uint32_t>(srcDimensions.y) % 2) << 1); // |= (1 << 1) if y is odd (2 or 3)
		
		MipGenerationData mipGenerationParams = MipGenerationData{
			.g_output0Dimensions = output0Dimensions,
			.g_mipParams = glm::uvec4(srcMipLevel, numMips, texParams.m_arraySize, 0.f),
			.g_resourceParams = glm::vec4(tex->IsSRGB(), srcDimensionMode, faceIdx, arrayIdx) };

		return mipGenerationParams;
	}
}


namespace gr
{
	ComputeMipsGraphicsSystem::ComputeMipsGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_stagePipeline(nullptr)
	{
	}


	void ComputeMipsGraphicsSystem::InitPipeline(re::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&)
	{
		m_stagePipeline = &pipeline;

		m_parentStageItr = m_stagePipeline->AppendRenderStage(
			re::RenderStage::CreateParentStage("MIP Generation Parent stage"));
	}


	void ComputeMipsGraphicsSystem::PreRender(DataDependencies const&)
	{
		std::vector<std::shared_ptr<re::Texture>> const& newTextures = 
			re::RenderManager::Get()->GetNewlyCreatedTextures();
		if (newTextures.empty())
		{
			return;
		}

		std::shared_ptr<re::Sampler> const mipSampler = re::Sampler::GetSampler("ClampMinMagLinearMipPoint");

		re::StagePipeline::StagePipelineItr insertItr = m_parentStageItr;

		for (std::shared_ptr<re::Texture> const& newTexture : newTextures)
		{
			re::Texture::TextureParams const& texParams = newTexture->GetTextureParams();
			if (texParams.m_mipMode != re::Texture::MipMode::AllocateGenerate)
			{
				continue;
			}

			SEAssert(texParams.m_dimension != re::Texture::Texture3D,
				"Texture3D Mip generation is not (currently) supported");

			const uint32_t totalMipLevels = newTexture->GetNumMips(); // Includes mip 0

			const uint32_t numFaces = re::Texture::GetNumFaces(newTexture.get());

			for (uint32_t arrayIdx = 0; arrayIdx < texParams.m_arraySize; arrayIdx++)
			{
				for (uint32_t faceIdx = 0; faceIdx < numFaces; faceIdx++)
				{
					constexpr uint32_t k_maxTargetsPerStage = 4;
					uint32_t targetMip = 1;
					while (targetMip < totalMipLevels)
					{
						const uint32_t firstTargetMipIdx = targetMip;
						const uint32_t sourceMip = targetMip - 1;

						const uint32_t numMipStages = targetMip + k_maxTargetsPerStage < totalMipLevels ? 
							k_maxTargetsPerStage : (totalMipLevels - targetMip);

						std::string const& stageName = std::format("Mip Gen: \"{}\" Array {}/{}, Face {}/{}, MIP {}-{}",
							newTexture->GetName().c_str(),
							arrayIdx + 1,
							texParams.m_arraySize,
							faceIdx + 1,
							numFaces,
							firstTargetMipIdx,
							firstTargetMipIdx + numMipStages - 1);

						std::shared_ptr<re::RenderStage> mipGenerationStage = 
							re::RenderStage::CreateSingleFrameComputeStage(
								stageName.c_str(),
								re::RenderStage::ComputeStageParams{});

						re::TextureView inputView;

						switch (texParams.m_dimension)
						{
						case re::Texture::Dimension::Texture1D:
						{
							inputView = re::TextureView::Texture1DView(sourceMip, 1);
						}
						break;
						case re::Texture::Dimension::Texture1DArray:
						{
							inputView = re::TextureView::Texture1DArrayView(sourceMip, 1, arrayIdx, 1);
						}
						break;
						case re::Texture::Dimension::Texture2D:
						{
							inputView = re::TextureView::Texture2DView(sourceMip, 1);
						}
						break;
						case re::Texture::Dimension::Texture2DArray:
						{
							inputView = re::TextureView::Texture2DArrayView(sourceMip, 1, arrayIdx, 1);
						}
						break;
						case re::Texture::Dimension::Texture3D:
						{
							inputView = re::TextureView::Texture3DView(sourceMip, 1, 0.f, arrayIdx, 1);
						}
						break;
						case re::Texture::Dimension::TextureCube:
						case re::Texture::Dimension::TextureCubeArray:
						{
							const uint32_t firstArraySlice = (arrayIdx * 6) + faceIdx;
							inputView = re::TextureView::Texture2DArrayView(sourceMip, 1, firstArraySlice, 1);
						}
						break;
						default: SEAssertF("Invalid dimension");
						}

						mipGenerationStage->AddPermanentTextureInput("SrcTex", newTexture, mipSampler, inputView);

						SEAssert(sourceMip != re::Texture::k_allMips, "Invalid source mip level");

						// Parameter buffer:
						MipGenerationData const& mipGenerationParams =
							CreateMipGenerationParamsData(newTexture, sourceMip, numMipStages, faceIdx, arrayIdx);

						mipGenerationStage->AddSingleFrameBuffer(re::Buffer::Create(
							MipGenerationData::s_shaderName,
							mipGenerationParams,
							re::Buffer::BufferParams{
								.m_type = re::Buffer::Type::SingleFrame,
								.m_memPoolPreference = re::Buffer::MemoryPoolPreference::Upload,
								.m_usageMask = re::Buffer::Usage::GPURead | re::Buffer::Usage::CPUWrite,
								.m_dataType = re::Buffer::DataType::Constant,
							}));

						// Set the drawstyle:
						switch (texParams.m_dimension)
						{
						case re::Texture::Dimension::Texture1D:
						case re::Texture::Dimension::Texture1DArray:
						{
							mipGenerationStage->SetDrawStyle(effect::drawstyle::TextureDimension_1D);
						}
						break;
						case re::Texture::Dimension::Texture2D:
						case re::Texture::Dimension::Texture2DArray:
						case re::Texture::Dimension::TextureCube:
						case re::Texture::Dimension::TextureCubeArray:
						{
							mipGenerationStage->SetDrawStyle(effect::drawstyle::TextureDimension_2D);
						}
						break;
						case re::Texture::Dimension::Texture3D:
						{
							mipGenerationStage->SetDrawStyle(effect::drawstyle::TextureDimension_3D);
						}
						break;
						default: SEAssertF("Invalid dimension");
						}

						// Attach our textures as UAVs:
						for (uint32_t currentTargetIdx = 0; currentTargetIdx < numMipStages; currentTargetIdx++)
						{
							re::TextureView textureView;

							switch (texParams.m_dimension)
							{
							case re::Texture::Dimension::Texture1D:
							{
								textureView = re::TextureView(
									re::TextureView::Texture1DView{ targetMip++, 1 });
							}
							break;
							case re::Texture::Dimension::Texture1DArray:
							{
								textureView = re::TextureView(
									re::TextureView::Texture1DArrayView{ targetMip++, 1, arrayIdx, 1 });
							}
							break;
							case re::Texture::Dimension::Texture2D:
							{
								textureView = re::TextureView(
									re::TextureView::Texture2DView{ targetMip++, 1, 0, 0.f });
							}
							break;
							case re::Texture::Dimension::Texture2DArray:
							{
								textureView = re::TextureView(
									re::TextureView::Texture2DArrayView{ targetMip++, 1, arrayIdx, 1, 0 });
							}
							break;
							case re::Texture::Dimension::Texture3D:
							{
								textureView = re::TextureView(
									re::TextureView::Texture3DView{ targetMip++, 1, 0.f, arrayIdx, 1 });
							}
							break;
							case re::Texture::Dimension::TextureCube:
							case re::Texture::Dimension::TextureCubeArray:
							{
								const uint32_t firstArraySlice = (arrayIdx * 6) + faceIdx;
								textureView = re::TextureView(
									re::TextureView::Texture2DArrayView{ targetMip++, 1, firstArraySlice, 1, 0 });
							}
							break;
							default: SEAssertF("Invalid dimension");
							}

							std::string const& shaderName = std::format("output{}", currentTargetIdx);

							mipGenerationStage->AddSingleFrameRWTextureInput(shaderName.c_str(), newTexture, textureView);
						}
					

						// We (currently) use 8x8 thread group dimensions
						constexpr uint32_t k_numThreadsX = 8;
						constexpr uint32_t k_numThreadsY = 8;

						// Non-integer MIP dimensions are rounded down to the nearest integer
						glm::vec2 subresourceDimensions = newTexture->GetMipLevelDimensions(firstTargetMipIdx).xy;
						subresourceDimensions = glm::floor(subresourceDimensions);

						const glm::uvec2 firstTargetMipDimensions = glm::uvec2(
							subresourceDimensions.x,
							subresourceDimensions.y);

						// We want to dispatch enough k_numThreadsX x k_numThreadsY threadgroups to cover every pixel in
						// our 1st mip level (each thread samples a 2x2 block in the source level above the 1st mip target)
						const uint32_t roundedXDim = std::max(util::RoundUpToNearestMultiple<uint32_t>(
							firstTargetMipDimensions.x / k_numThreadsX, k_numThreadsX), 1u);
						const uint32_t roundedYDim = std::max(util::RoundUpToNearestMultiple<uint32_t>(
							firstTargetMipDimensions.y / k_numThreadsY, k_numThreadsY), 1u);

						// Add our dispatch information to a compute batch:
						re::Batch computeBatch = re::Batch(
							re::Lifetime::SingleFrame,
							re::Batch::ComputeParams{ .m_threadGroupCount = glm::uvec3(roundedXDim, roundedYDim, 1u) },
							effect::Effect::ComputeEffectID("MipGeneration"));

						mipGenerationStage->AddBatch(computeBatch);

						insertItr = m_stagePipeline->AppendSingleFrameRenderStage(insertItr, std::move(mipGenerationStage));
					}
				}
			}
		}
	}
}
