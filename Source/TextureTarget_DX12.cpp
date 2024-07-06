// © 2022 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "SysInfo_DX12.h"
#include "TextureTarget_DX12.h"
#include "Texture_DX12.h"

#include "Core/Assert.h"
#include "Core/Config.h"

#include "Core/Util/CastUtils.h"

#include <d3dx12.h>

using Microsoft::WRL::ComPtr;


namespace
{
	void CreateViewportAndScissorRect(re::TextureTargetSet const& targetSet)
	{
		dx12::TextureTargetSet::PlatformParams* texTargetSetPlatParams =
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

		// Configure the viewport:
		re::Viewport const& viewport = targetSet.GetViewport();

		texTargetSetPlatParams->m_viewport = CD3DX12_VIEWPORT(
			static_cast<float>(viewport.xMin()),
			static_cast<float>(viewport.yMin()),
			static_cast<float>(viewport.Width()),
			static_cast<float>(viewport.Height()));

		// Configure the scissor rectangle:
		re::ScissorRect const& scissorRect = targetSet.GetScissorRect();

		SEAssert(util::CheckedCast<uint32_t>(scissorRect.Left()) >= targetSet.GetViewport().xMin() &&
			util::CheckedCast<uint32_t>(scissorRect.Top()) >= targetSet.GetViewport().yMin() &&
			util::CheckedCast<uint32_t>(scissorRect.Right()) <= targetSet.GetViewport().Width() &&
			util::CheckedCast<uint32_t>(scissorRect.Bottom()) <= targetSet.GetViewport().Height(),
			"Scissor rectangle is out of bounds of the viewport");

		texTargetSetPlatParams->m_scissorRect = CD3DX12_RECT(
			scissorRect.Left(),
			scissorRect.Top(),
			scissorRect.Right(),
			scissorRect.Bottom());
	}
}

namespace dx12
{
	void TextureTargetSet::CreateColorTargets(re::TextureTargetSet const& targetSet)
	{
		if (!targetSet.HasColorTarget())
		{
			return;
		}

		dx12::TextureTargetSet::PlatformParams* texTargetSetPlatParams =
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();
		SEAssert(texTargetSetPlatParams->m_isCommitted, "Target set has not been committed");

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();
		
		for (re::TextureTarget const& colorTarget : targetSet.GetColorTargets())
		{
			if (!colorTarget.HasTexture())
			{
				break;
			}

			dx12::TextureTarget::PlatformParams* targetPlatParams =
				colorTarget.GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();
			SEAssert(!targetPlatParams->m_isCreated, "Target has already been created");
			targetPlatParams->m_isCreated = true;

			re::Texture* colorTex = colorTarget.GetTexture().get();

			re::Texture::TextureParams const& texParams = colorTex->GetTextureParams();

			// Create RTVs:
			if ((texParams.m_usage & re::Texture::Usage::ColorTarget) || 
				(texParams.m_usage & re::Texture::Usage::SwapchainColorProxy))
			{				
				dx12::Texture::PlatformParams* texPlatParams =
					colorTex->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
				
				SEAssert(texPlatParams->m_isCreated && texPlatParams->m_textureResource, "Texture is not created");

				re::TextureTarget::TargetParams const& targetParams = colorTarget.GetTargetParams();

				SEAssert(targetPlatParams->m_subresourceDescriptors.IsValid() == false,
					"RTVs have already been allocated. This is unexpected");

				// Create per-subresource RTVs:
				const uint32_t arraySize = texParams.m_arraySize;
				const uint32_t numFaces = texParams.m_faces;
				const uint32_t numMips = colorTex->GetNumMips();

				const uint32_t numSubresourceDescriptors = colorTex->GetTotalNumSubresources();

				SEAssert(targetParams.m_targetFace < numFaces && (numFaces == 1 || numFaces == 6),
					"Invalid face configuration");

				SEAssert(numFaces == 1 ||
					(numFaces == 6 &&
						(texParams.m_dimension == re::Texture::Dimension::TextureCubeMap ||
							texParams.m_dimension == re::Texture::Dimension::TextureCubeMapArray)),
					"Invalid face/dimension configuration");

				targetPlatParams->m_subresourceDescriptors = std::move(context->GetCPUDescriptorHeapMgr(
					CPUDescriptorHeapManager::HeapType::RTV).Allocate(numSubresourceDescriptors));
				SEAssert(targetPlatParams->m_subresourceDescriptors.IsValid(), "RTV descriptor is not valid");

				for (uint32_t arrayIdx = 0; arrayIdx < arraySize; arrayIdx++)
				{
					for (uint32_t faceIdx = 0; faceIdx < numFaces; faceIdx++)
					{
						for (uint32_t mipIdx = 0; mipIdx < numMips; mipIdx++)
						{
							// Create the RTV:
							D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
							renderTargetViewDesc.Format = texPlatParams->m_format;

							switch (texParams.m_dimension)
							{
							case re::Texture::Texture1D:
							{
								SEAssert(texParams.m_arraySize == 1, "Unexpected array size");

								renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;

								renderTargetViewDesc.Texture1D = D3D12_TEX1D_RTV
								{
									.MipSlice = mipIdx,
								};
							}
							break;
							case re::Texture::Texture1DArray:
							{
								renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;

								renderTargetViewDesc.Texture1DArray = D3D12_TEX1D_ARRAY_RTV
								{
									.MipSlice = mipIdx,
									.FirstArraySlice = arrayIdx,
									.ArraySize = 1,
								};
							}
							break;
							case re::Texture::Texture2D:
							{
								SEAssert(texParams.m_arraySize == 1 && texParams.m_faces == 1, "Unexpected size params");

								switch (texParams.m_multisampleMode)
								{
								case re::Texture::MultisampleMode::Disabled:
								{
									renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

									renderTargetViewDesc.Texture2D = D3D12_TEX2D_RTV
									{
										.MipSlice = targetParams.m_targetMip,
										.PlaneSlice = 0
									};
								}
								break;
								case re::Texture::MultisampleMode::Enabled:
								{
									SEAssertF("TODO: Support multisampling");
								}
								break;
								default: SEAssertF("Invalid multisample mode");
								}
							}
							break;
							case re::Texture::Texture2DArray:
							{
								SEAssert(texParams.m_faces == 1, "Unexpected configuration");

								switch (texParams.m_multisampleMode)
								{
								case re::Texture::MultisampleMode::Disabled:
								{
									renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
									
									renderTargetViewDesc.Texture2DArray = D3D12_TEX2D_ARRAY_RTV
									{
										.MipSlice = targetParams.m_targetMip,
										.FirstArraySlice = arrayIdx,
										.ArraySize = 1,
										.PlaneSlice = arrayIdx,
									};
								}
								break;
								case re::Texture::MultisampleMode::Enabled:
								{
									SEAssertF("TODO: Support multisampling");
								}
								break;
								default: SEAssertF("Invalid multisample mode");
								}
							}
							break;
							case re::Texture::Texture3D:
							{
								SEAssert(texParams.m_faces == 1, "Unexpected configuration");

								const uint32_t firstWSlice = arraySize - arrayIdx;

								renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;

								renderTargetViewDesc.Texture3D = D3D12_TEX3D_RTV
								{
									.MipSlice = targetParams.m_targetMip,
									.FirstWSlice = firstWSlice,
									.WSize = static_cast<uint32_t>(-1),  // -1 = all slices from FirstWSlice to the last slice
								};
							}
							break;
							case re::Texture::TextureCubeMap:
							{
								SEAssert(arraySize == 1 && numFaces == 6, "Unexpected array size or number of faces");

								const uint32_t firstArraySlice = (arrayIdx * numFaces) + faceIdx;

								renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;

								renderTargetViewDesc.Texture2DArray = D3D12_TEX2D_ARRAY_RTV
								{
									.MipSlice = mipIdx,	// Mip slices include 1 mip level for every texture in an array
									.FirstArraySlice = firstArraySlice,
									.ArraySize = 1,	// Only view one element of our array
									.PlaneSlice = 0	// "Only Plane Slice 0 is valid when creating a view on a non-planar format"
								};
							}
							break;
							case re::Texture::TextureCubeMapArray:
							{
								SEAssert(numFaces == 6, "Unexpected number of faces");

								const uint32_t firstArraySlice = (arrayIdx * numFaces) + faceIdx;

								renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;

								renderTargetViewDesc.Texture2DArray = D3D12_TEX2D_ARRAY_RTV
								{
									.MipSlice = mipIdx,	// Mip slices include 1 mip level for every texture in an array
									.FirstArraySlice = firstArraySlice,
									.ArraySize = 1,	// Only view one element of our array
									.PlaneSlice = 0	// "Only Plane Slice 0 is valid when creating a view on a non-planar format"
								};
							}
							break;
							default: SEAssertF("Invalid texture dimension for a depth target");
							}

							const uint32_t descriptorIdx = 
								dx12::TextureTarget::GetTargetDescriptorIndex(colorTex, arrayIdx, faceIdx, mipIdx);

							device->CreateRenderTargetView(
								texPlatParams->m_textureResource.Get(),
								&renderTargetViewDesc,
								targetPlatParams->m_subresourceDescriptors[descriptorIdx]);
						}
					}
				}

				// Cubemap descriptors:
				if (texParams.m_dimension == re::Texture::Dimension::TextureCubeMap ||
					texParams.m_dimension == re::Texture::Dimension::TextureCubeMapArray)
				{
					SEAssert(targetPlatParams->m_cubemapDescriptors.IsValid() == false,
						"Cubemap RTVs have already been allocated. This is unexpected");

					SEAssert(numFaces == 6, "Unexpected number of faces");

					const uint32_t numCubemapDescriptors = 
						dx12::TextureTarget::GetNumRequiredCubemapTargetDescriptors(colorTex);

					targetPlatParams->m_cubemapDescriptors = std::move(context->GetCPUDescriptorHeapMgr(
						CPUDescriptorHeapManager::HeapType::RTV).Allocate(numCubemapDescriptors));
					SEAssert(targetPlatParams->m_cubemapDescriptors.IsValid(), "Cubemap RTV descriptors are not valid");

					// Create a descriptor to view the whole cubemap for each mip level of each array element
					for (uint32_t arrayIdx = 0; arrayIdx < arraySize; arrayIdx++)
					{
						for (uint32_t mipIdx = 0; mipIdx < numMips; mipIdx++)
						{
							D3D12_RENDER_TARGET_VIEW_DESC cubemapViewDesc{};
							cubemapViewDesc.Format = texPlatParams->m_format;
							cubemapViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;

							const uint32_t firstArraySlice = (arrayIdx * numFaces);

							cubemapViewDesc.Texture2DArray = D3D12_TEX2D_ARRAY_RTV
							{
								.MipSlice = mipIdx,	// Mip slices include 1 mip level for every texture in an array
								.FirstArraySlice = firstArraySlice,
								.ArraySize = numFaces,
								.PlaneSlice = 0	// "Only Plane Slice 0 is valid when creating a view on a non-planar format"
							};

							const uint32_t descriptorIdx = dx12::TextureTarget::GetTargetDescriptorIndex(
								colorTex, arrayIdx, re::Texture::k_allFaces, mipIdx);

							device->CreateRenderTargetView(
								texPlatParams->m_textureResource.Get(),
								&cubemapViewDesc,
								targetPlatParams->m_cubemapDescriptors[descriptorIdx]);
						}
					}
				}
			}
		}

		CreateViewportAndScissorRect(targetSet);
	}


	void TextureTargetSet::CreateDepthStencilTarget(re::TextureTargetSet const& targetSet)
	{
		if (!targetSet.HasDepthTarget())
		{
			return;
		}

		SEAssert(targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>()->m_isCommitted,
			"Target set has not been committed");

		re::TextureTarget const* depthTarget = targetSet.GetDepthStencilTarget();

		dx12::TextureTarget::PlatformParams* depthTargetPlatParams =
			depthTarget->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();
		SEAssert(!depthTargetPlatParams->m_isCreated, "Target has already been created");
		depthTargetPlatParams->m_isCreated = true;

		re::Texture const* depthTex = depthTarget->GetTexture().get();
		re::Texture::TextureParams const& depthTexParams = depthTex->GetTextureParams();
		SEAssert(depthTexParams.m_usage & re::Texture::Usage::DepthTarget,
			"Target does not have the depth target usage type");

		dx12::Texture::PlatformParams const* depthTexPlatParams =
			depthTex->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();
		SEAssert(depthTexPlatParams->m_isCreated && depthTexPlatParams->m_textureResource,
			"Depth texture has not been created");

		// If we don't have any color targets, we must configure the viewport and scissor rect here instead
		if (!targetSet.HasColorTarget())
		{
			CreateViewportAndScissorRect(targetSet);
		}

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		SEAssert(depthTargetPlatParams->m_subresourceDescriptors.IsValid() == false,
			"DSVs have already been allocated. This is unexpected");

		const uint32_t arraySize = depthTexParams.m_arraySize;
		const uint32_t numFaces = depthTexParams.m_faces;
		const uint32_t numMips = depthTex->GetNumMips();

		SEAssert(numMips == 1, "Depth texture has mips. This is unexpected");

		const uint32_t numSubresourceDescriptors = depthTex->GetTotalNumSubresources();

		depthTargetPlatParams->m_subresourceDescriptors = std::move(context->GetCPUDescriptorHeapMgr(
			CPUDescriptorHeapManager::HeapType::DSV).Allocate(numSubresourceDescriptors));
		SEAssert(depthTargetPlatParams->m_subresourceDescriptors.IsValid(), "DSV descriptor is not valid");

		re::TextureTarget::TargetParams const& targetParams = depthTarget->GetTargetParams();

		// Create per-subresource DSVs:
		for (uint32_t arrayIdx = 0; arrayIdx < arraySize; arrayIdx++)
		{
			for (uint32_t faceIdx = 0; faceIdx < numFaces; faceIdx++)
			{
				for (uint32_t mipIdx = 0; mipIdx < numMips; mipIdx++)
				{
					D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
					dsv.Format = depthTexPlatParams->m_format;
					dsv.Flags = D3D12_DSV_FLAG_NONE;

					switch (depthTexParams.m_dimension)
					{
					case re::Texture::Texture1D:
					{
						SEAssert(depthTexParams.m_arraySize == 1 &&
							depthTexParams.m_faces == 1,
							"Unexpected configuration");

						dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;

						dsv.Texture1D.MipSlice = mipIdx;
					}
					break;
					case re::Texture::Texture1DArray:
					{
						SEAssert(depthTexParams.m_faces == 1, "Unexpected configuration");

						dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;

						dsv.Texture1DArray.MipSlice = mipIdx;
						dsv.Texture1DArray.FirstArraySlice = arrayIdx;
						dsv.Texture1DArray.ArraySize = 1;
					}
					break;
					case re::Texture::Texture2D:
					{
						SEAssert(depthTexParams.m_arraySize == 1 &&
							depthTexParams.m_faces == 1,
							"Unexpected size params");

						switch (depthTexParams.m_multisampleMode)
						{
						case re::Texture::MultisampleMode::Disabled:
						{
							dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

							dsv.Texture2D.MipSlice = targetParams.m_targetMip;
						}
						break;
						case re::Texture::MultisampleMode::Enabled:
						{
							SEAssertF("TODO: Support multisampling");
						}
						break;
						default: SEAssertF("Invalid multisample mode");
						}						
					}
					break;
					case re::Texture::Texture2DArray:
					{
						SEAssert(depthTexParams.m_faces == 1, "Unexpected configuration");

						switch (depthTexParams.m_multisampleMode)
						{
						case re::Texture::MultisampleMode::Disabled:
						{
							dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;

							dsv.Texture2DArray.MipSlice = mipIdx;
							dsv.Texture2DArray.FirstArraySlice = arrayIdx;
							dsv.Texture2DArray.ArraySize = 1;
						}
						break;
						case re::Texture::MultisampleMode::Enabled:
						{
							SEAssertF("TODO: Support multisampling");
						}
						break;
						default: SEAssertF("Invalid multisample mode");
						}
					}
					break;
					case re::Texture::TextureCubeMap:
					{
						SEAssert(arraySize == 1 && numFaces == 6, "Unexpected array size or number of faces");

						const uint32_t firstArraySlice = (arrayIdx * numFaces) + faceIdx;

						dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;

						dsv.Texture2DArray.MipSlice = mipIdx; // Mip slices include 1 mip level for every texture in an array
						dsv.Texture2DArray.FirstArraySlice = firstArraySlice;	// Only view one element of our array
						dsv.Texture2DArray.ArraySize = 1; // Target: Only view one element of our array
					}
					break;
					case re::Texture::TextureCubeMapArray:
					{
						SEAssert(numFaces == 6, "Unexpected number of faces");

						const uint32_t firstArraySlice = (arrayIdx * numFaces) + faceIdx;

						dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;

						dsv.Texture2DArray.MipSlice = mipIdx; // Mip slices include 1 mip level for every texture in an array
						dsv.Texture2DArray.FirstArraySlice = firstArraySlice;	// Only view one element of our array
						dsv.Texture2DArray.ArraySize = 1;
					}
					break;
					case re::Texture::Texture3D:
					default: SEAssertF("Invalid texture dimension for a depth target");
					}

					const uint32_t descriptorIdx = 
						dx12::TextureTarget::GetTargetDescriptorIndex(depthTex, arrayIdx, faceIdx, mipIdx);

					device->CreateDepthStencilView(
						depthTexPlatParams->m_textureResource.Get(),
						&dsv,
						depthTargetPlatParams->m_subresourceDescriptors[descriptorIdx]);
				}
			}
		}

		// Create a DSV for all cube map faces at once
		if (depthTexParams.m_dimension == re::Texture::Dimension::TextureCubeMap ||
			depthTexParams.m_dimension == re::Texture::Dimension::TextureCubeMapArray)
		{
			SEAssert(depthTargetPlatParams->m_cubemapDescriptors.IsValid() == false,
				"Cubemap DSVs have already been allocated. This is unexpected");

			SEAssert(numFaces == 6, "Unexpected number of faces");

			const uint32_t numCubemapDescriptors = 
				dx12::TextureTarget::GetNumRequiredCubemapTargetDescriptors(depthTex);

			depthTargetPlatParams->m_cubemapDescriptors = std::move(context->GetCPUDescriptorHeapMgr(
				CPUDescriptorHeapManager::HeapType::DSV).Allocate(numCubemapDescriptors));
			SEAssert(depthTargetPlatParams->m_cubemapDescriptors.IsValid(), "Cube DSV descriptors are not valid");

			// Create a descriptor to view the whole cubemap for each mip level of each array element
			for (uint32_t arrayIdx = 0; arrayIdx < arraySize; arrayIdx++)
			{
				for (uint32_t mipIdx = 0; mipIdx < numMips; mipIdx++)
				{
					D3D12_DEPTH_STENCIL_VIEW_DESC cubeDsv = {};
					cubeDsv.Format = depthTexPlatParams->m_format;
					cubeDsv.Flags = D3D12_DSV_FLAG_NONE;
					cubeDsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;

					const uint32_t firstArraySlice = (arrayIdx * numFaces);

					cubeDsv.Texture2DArray = D3D12_TEX2D_ARRAY_DSV
					{
						.MipSlice = mipIdx,
						.FirstArraySlice = firstArraySlice,
						.ArraySize = numFaces,
					};

					const uint32_t descriptorIdx = dx12::TextureTarget::GetTargetDescriptorIndex(
						depthTex, arrayIdx, re::Texture::k_allFaces, mipIdx);

					device->CreateDepthStencilView(
						depthTexPlatParams->m_textureResource.Get(),
						&cubeDsv,
						depthTargetPlatParams->m_cubemapDescriptors[descriptorIdx]);
				}
			}			
		}
	}


	D3D12_RT_FORMAT_ARRAY TextureTargetSet::GetColorTargetFormats(re::TextureTargetSet const& targetSet)
	{
		// Note: We pack our structure with contiguous DXGI_FORMAT's, regardless of their packing in the 
		// re::TextureTargetSet slots
		D3D12_RT_FORMAT_ARRAY colorTargetFormats{};
		uint32_t numTargets = 0;
		for (uint8_t i = 0; i < targetSet.GetColorTargets().size(); i++)
		{
			if (!targetSet.GetColorTarget(i).HasTexture())
			{
				break;
			}

			dx12::Texture::PlatformParams const* targetTexPlatParams =
				targetSet.GetColorTarget(i).GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

			colorTargetFormats.RTFormats[i] = targetTexPlatParams->m_format;
			numTargets++;
		}
		SEAssert(numTargets > 0, "No color targets found");
		colorTargetFormats.NumRenderTargets = numTargets;

		return colorTargetFormats;
	}


	uint32_t TextureTarget::GetTargetDescriptorIndex(
		re::Texture const* texture, uint32_t arrayIdx, uint32_t faceIdx, uint32_t mipIdx)
	{
		re::Texture::TextureParams const& texParams = texture->GetTextureParams();

		const uint32_t numMips = texture->GetNumMips();

		if ((texParams.m_dimension == re::Texture::TextureCubeMap ||
			texParams.m_dimension == re::Texture::TextureCubeMapArray) &&
			faceIdx == re::Texture::k_allFaces)
		{
			SEAssert(arrayIdx < texParams.m_arraySize && mipIdx < numMips,
				"OOB cubemap descriptor index");

			return (arrayIdx * numMips) + mipIdx;
		}
		else
		{
			SEAssert(arrayIdx < texParams.m_arraySize && faceIdx < texParams.m_faces && mipIdx < numMips,
				"OOB target descriptor index");

			return texture->GetSubresourceIndex(arrayIdx, faceIdx, mipIdx);
		}
	}


	D3D12_CPU_DESCRIPTOR_HANDLE TextureTarget::GetTargetDescriptor(re::TextureTarget const& texTarget)
	{
		SEAssert(texTarget.HasTexture(), "Trying to get a descriptor for a target with no texture");

		re::TextureTarget::TargetParams const& targetParams = texTarget.GetTargetParams();

		dx12::TextureTarget::PlatformParams* targetPlatParams =
			texTarget.GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();

		re::Texture const* texture = texTarget.GetTexture().get();
		re::Texture::TextureParams const& texParams = texture->GetTextureParams();

		dx12::DescriptorAllocation const* descriptors = nullptr;

		// TODO: Allow selection of DSVs created with depth writes disabled
		if ((texParams.m_dimension == re::Texture::TextureCubeMap ||
			texParams.m_dimension == re::Texture::TextureCubeMapArray) &&
			targetParams.m_targetFace == re::Texture::k_allFaces)
		{
			descriptors = &targetPlatParams->m_cubemapDescriptors;
		}
		else
		{
			descriptors = &targetPlatParams->m_subresourceDescriptors;
		}

		const uint32_t descriptorIdx = dx12::TextureTarget::GetTargetDescriptorIndex(
			texture,
			targetParams.m_targetArrayIdx,
			targetParams.m_targetFace,
			targetParams.m_targetMip);

		return (*descriptors)[descriptorIdx];
	}


	uint32_t TextureTarget::GetNumRequiredCubemapTargetDescriptors(re::Texture const* texture)
	{
		re::Texture::TextureParams const& texParams = texture->GetTextureParams();

		if (texParams.m_dimension == re::Texture::TextureCubeMap ||
			texParams.m_dimension == re::Texture::TextureCubeMapArray)
		{
			SEAssert(texParams.m_faces == 6, "Unexpected number of faces");

			return texParams.m_arraySize * texture->GetNumMips();
		}
		return 0;
	}
}