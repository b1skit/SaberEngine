// © 2024 Adam Badke. All rights reserved.
#include "TextureView.h"


namespace re
{
	TextureView::TextureView(TextureView::Texture1DView const& view, ViewFlags const& flags /*= ViewFlags{}*/)
		: m_viewDimension(re::Texture::Texture1D)
		, Texture1D{ view }
		, Flags(flags)
	{
		AddDataBytesToHash<uint8_t>(re::Texture::Texture1D);
		AddDataBytesToHash<Texture1DView>(Texture1D);
		AddDataBytesToHash<ViewFlags>(Flags);
	};


	TextureView::TextureView(TextureView::Texture1DArrayView const& view, ViewFlags const& flags /*= ViewFlags{}*/)
		: m_viewDimension(re::Texture::Texture1DArray)
		, Texture1DArray{ view }
		, Flags(flags)
	{
		AddDataBytesToHash<uint8_t>(re::Texture::Texture1DArray);
		AddDataBytesToHash<Texture1DArrayView>(Texture1DArray);
		AddDataBytesToHash<ViewFlags>(Flags);
	};


	TextureView::TextureView(TextureView::Texture2DView const& view, ViewFlags const& flags /*= ViewFlags{}*/)
		: m_viewDimension(re::Texture::Texture2D)
		, Texture2D{ view }
		, Flags(flags)
	{
		AddDataBytesToHash<uint8_t>(re::Texture::Texture2D);
		AddDataBytesToHash<Texture2DView>(Texture2D);
		AddDataBytesToHash<ViewFlags>(Flags);
	};


	TextureView::TextureView(TextureView::Texture2DArrayView const& view, ViewFlags const& flags /*= ViewFlags{}*/)
		: m_viewDimension(re::Texture::Texture2DArray)
		, Texture2DArray{ view }
		, Flags(flags)
	{
		AddDataBytesToHash<uint8_t>(re::Texture::Texture2DArray);
		AddDataBytesToHash<Texture2DArrayView>(Texture2DArray);
		AddDataBytesToHash<ViewFlags>(Flags);
	};


	TextureView::TextureView(TextureView::Texture3DView const& view, ViewFlags const& flags /*= ViewFlags{}*/)
		: m_viewDimension(re::Texture::Texture3D)
		, Texture3D{ view }
		, Flags(flags)
	{
		AddDataBytesToHash<uint8_t>(re::Texture::Texture3D);
		AddDataBytesToHash<Texture3DView>(Texture3D);
		AddDataBytesToHash<ViewFlags>(Flags);
	};


	TextureView::TextureView(TextureView::TextureCubeView const& view, ViewFlags const& flags /*= ViewFlags{}*/)
		: m_viewDimension(re::Texture::TextureCube)
		, TextureCube{ view }
		, Flags(flags)
	{
		AddDataBytesToHash<uint8_t>(re::Texture::TextureCube);
		AddDataBytesToHash<TextureCubeView>(TextureCube);
		AddDataBytesToHash<ViewFlags>(Flags);
	};


	TextureView::TextureView(TextureView::TextureCubeArrayView const& view, ViewFlags const& flags /*= ViewFlags{}*/)
		: m_viewDimension(re::Texture::TextureCubeArray)
		, TextureCubeArray{ view }
		, Flags(flags)
	{
		AddDataBytesToHash<uint8_t>(re::Texture::TextureCubeArray);
		AddDataBytesToHash<TextureCubeArrayView>(TextureCubeArray);
		AddDataBytesToHash<ViewFlags>(Flags);
	};


	TextureView::TextureView(re::Texture const* tex) // Create a default view including all subresources
		: m_viewDimension(re::Texture::Dimension_Invalid)
	{
		*this = CreateDefaultView(*tex);
	};


	TextureView::TextureView(re::Texture const& tex) // Create a default view including all subresources
		: m_viewDimension(re::Texture::Dimension_Invalid)
	{
		*this = CreateDefaultView(tex);
	};


	TextureView::TextureView(std::shared_ptr<re::Texture const> const& tex) // Create a default view including all subresources
		: m_viewDimension(re::Texture::Dimension_Invalid)
	{
		*this = CreateDefaultView(*tex.get());
	};


	TextureView::TextureView()
		: m_viewDimension(re::Texture::Dimension_Invalid)
	{
		/* Don't use this directly */
	};


	TextureView::TextureView(TextureView const& rhs) noexcept
		: m_viewDimension(rhs.m_viewDimension)
	{
		*this = rhs;
	}


	TextureView::TextureView(TextureView&& rhs) noexcept
		: m_viewDimension(rhs.m_viewDimension)
	{
		*this = rhs;
	}


	TextureView& TextureView::operator=(TextureView const& rhs) noexcept
	{
		if (this != &rhs)
		{
			memcpy(this, &rhs, sizeof(TextureView));
		}
		return *this;
	}

	TextureView& TextureView::operator=(TextureView&& rhs) noexcept
	{
		if (this != &rhs)
		{
			memcpy(this, &rhs, sizeof(TextureView));
		}
		return *this;
	}


	uint32_t TextureView::GetSubresourceIndex(re::Texture const* texture, TextureView const& texView)
	{
		re::Texture::TextureParams const& texParams = texture->GetTextureParams();

		const uint32_t numMips = texture->GetNumMips();

		switch (texView.m_viewDimension)
		{
		case re::Texture::Dimension::Texture1D:
		{
			SEAssert(texView.Texture1D.m_mipLevels == 1, "View describes more than 1 subresource");

			return texView.Texture1D.m_firstMip;
		}
		break;
		case re::Texture::Dimension::Texture1DArray:
		{
			SEAssert(texView.Texture1DArray.m_mipLevels == 1 && texView.Texture1DArray.m_arraySize == 1,
				"View describes more than 1 subresource");
			
			return (texView.Texture1DArray.m_firstArraySlice * numMips) + texView.Texture1DArray.m_firstMip;
		}
		break;
		case re::Texture::Dimension::Texture2D:
		{
			SEAssert(texView.Texture2D.m_mipLevels == 1, "View describes more than 1 subresource");
			SEAssert(texView.Texture2D.m_planeSlice == 0, "TODO: Support multi-plane formats");

			return texView.Texture2D.m_firstMip;
		}
		break;
		case re::Texture::Dimension::Texture2DArray:
		{
			SEAssert(texView.Texture2DArray.m_mipLevels == 1 && texView.Texture2DArray.m_arraySize == 1,
				"View describes more than 1 subresource");
			SEAssert(texView.Texture2DArray.m_planeSlice == 0, "TODO: Support multi-plane formats");

			return (numMips * texView.Texture2DArray.m_firstArraySlice) + texView.Texture2DArray.m_firstMip;
		}
		break;
		case re::Texture::Dimension::Texture3D:
		{
			SEAssert(texView.Texture3D.m_mipLevels == 1, "View describes more than 1 subresource");

			return texView.Texture3D.m_firstMip;
		}
		break;
		case re::Texture::Dimension::TextureCube:
		case re::Texture::Dimension::TextureCubeArray:
		{
			SEAssertF("Cubemap views describe more than 1 subresource at at time");
		}
		break;
		default: SEAssertF("Invalid dimension");
		}

		return 0; // This should never happen
	}


	uint32_t TextureView::GetSubresourceIndexFromRelativeOffsets(
		re::Texture const* tex, TextureView const& texView, uint32_t relativeArrayIdx, uint32_t relativeMipIdx)
	{
		// NOTE: Array/mip indexes are RELATIVE to the 1st array/mip index in the view

		re::Texture::TextureParams const& texParams = tex->GetTextureParams();

		const uint32_t numMips = tex->GetNumMips();

		uint32_t subresourceIdx = std::numeric_limits<uint32_t>::max();

		switch (texView.m_viewDimension)
		{
		case re::Texture::Dimension::Texture1D:
		{
			SEAssert(relativeArrayIdx == 0, "Invalid array index");
			SEAssert(texView.Texture1D.m_firstMip + relativeMipIdx < numMips, "Result is OOB");

			subresourceIdx =  texView.Texture1D.m_firstMip + relativeMipIdx;
		}
		break;
		case re::Texture::Dimension::Texture1DArray:
		{
			SEAssert(texView.Texture1DArray.m_firstArraySlice + relativeArrayIdx < texParams.m_arraySize &&
				texView.Texture1DArray.m_firstMip + relativeMipIdx < numMips,
				"Result is OOB");

			const uint32_t arrayOffset = texView.Texture1DArray.m_firstArraySlice + relativeArrayIdx;
			const uint32_t mipOffset = texView.Texture1DArray.m_firstMip + relativeMipIdx;

			subresourceIdx = (arrayOffset * numMips) + mipOffset;
		}
		break;
		case re::Texture::Dimension::Texture2D:
		{
			SEAssert(relativeArrayIdx == 0, "Invalid array index");
			SEAssert(texView.Texture2D.m_planeSlice == 0, "TODO: Support multi-plane formats");
			SEAssert(texView.Texture2D.m_firstMip + relativeMipIdx < numMips, "Result is OOB");

			subresourceIdx = texView.Texture2D.m_firstMip + relativeMipIdx;
		}
		break;
		case re::Texture::Dimension::Texture2DArray:
		{
			// Texture2DArray is also used by cubemaps
			switch (texParams.m_dimension)
			{
			case re::Texture::Texture2D:
			case re::Texture::Texture2DArray:
			{
				subresourceIdx = tex->GetSubresourceIndex(
					texView.Texture2DArray.m_firstArraySlice + relativeArrayIdx,
					0,
					texView.Texture2DArray.m_firstMip + relativeMipIdx);
			}
			break;
			case re::Texture::TextureCube:
			case re::Texture::TextureCubeArray:
			{
				const uint32_t firstArraySliceIdx = texView.Texture2DArray.m_firstArraySlice * numMips;
				const uint32_t firstSubresourceIdx = firstArraySliceIdx + (relativeArrayIdx * numMips);

				subresourceIdx = firstSubresourceIdx + texView.Texture2DArray.m_firstMip + relativeMipIdx;
			}
			break;
			default: SEAssertF("Invalid dimension");
			}
		}
		break;
		case re::Texture::Dimension::Texture3D:
		{
			SEAssert(texView.Texture3D.m_firstMip + relativeMipIdx < numMips, "Result is OOB");

			subresourceIdx = texView.Texture3D.m_firstMip + relativeMipIdx;
		}
		break;
		case re::Texture::Dimension::TextureCube:
		case re::Texture::Dimension::TextureCubeArray:
		{
			SEAssertF("Cubemap views describe more than 1 subresource at at time");
		}
		break;
		default: SEAssertF("Invalid dimension");
		}

		SEAssert(subresourceIdx < tex->GetTotalNumSubresources(), "Subresource index is OOB");

		return subresourceIdx;
	}


	std::vector<uint32_t> TextureView::GetSubresourceIndexes(re::Texture const* texture, re::TextureView const& texView)
	{
		std::vector<uint32_t> subresourceIndexes;

		re::Texture::TextureParams const& texParams = texture->GetTextureParams();

		const uint32_t numMips = texture->GetNumMips();

		uint32_t totalSubresources = 0;
		switch (texView.m_viewDimension)
		{
		case re::Texture::Texture1D:
		{
			SEAssert(texView.Texture1D.m_firstMip < numMips &&
				(texView.Texture1D.m_mipLevels == re::Texture::k_allMips ||
					texView.Texture1D.m_firstMip + texView.Texture1D.m_mipLevels <= numMips),
				"Indexes are out of bounds");

			totalSubresources = texView.Texture1D.m_mipLevels;
			if (texView.Texture1D.m_mipLevels == re::Texture::k_allMips)
			{
				totalSubresources = numMips - texView.Texture1D.m_firstMip;
			}
			subresourceIndexes.reserve(totalSubresources);

			for (uint32_t relMipIdx = 0; relMipIdx < totalSubresources; ++relMipIdx)
			{
				subresourceIndexes.emplace_back(re::TextureView::GetSubresourceIndexFromRelativeOffsets(
					texture, 
					texView, 
					0, 
					relMipIdx));
			}
		}
		break;
		case re::Texture::Texture1DArray:
		{
			SEAssert(texView.Texture1DArray.m_firstMip < numMips &&
				(texView.Texture1DArray.m_mipLevels == re::Texture::k_allMips ||
					texView.Texture1DArray.m_firstMip + texView.Texture1DArray.m_mipLevels <= numMips) &&
				texView.Texture1DArray.m_arraySize > 0 &&
				texView.Texture1DArray.m_firstArraySlice < texParams.m_arraySize &&
				texView.Texture1DArray.m_firstArraySlice + texView.Texture1DArray.m_arraySize <= texParams.m_arraySize,
				"Indexes are out of bounds");

			uint32_t totalMips = texView.Texture1DArray.m_mipLevels;
			if (texView.Texture1DArray.m_mipLevels == re::Texture::k_allMips)
			{
				totalMips = numMips - texView.Texture1DArray.m_firstMip;
			}

			totalSubresources = texView.Texture1DArray.m_arraySize * totalMips;

			subresourceIndexes.reserve(totalSubresources);

			for (uint32_t relArrayIdx = 0; relArrayIdx < texView.Texture1DArray.m_arraySize; ++relArrayIdx)
			{
				for (uint32_t relMipIdx = 0; relMipIdx < totalMips; ++relMipIdx)
				{
					subresourceIndexes.emplace_back(re::TextureView::GetSubresourceIndexFromRelativeOffsets(
						texture, 
						texView, 
						relArrayIdx,
						relMipIdx));
				}
			}
		}
		break;
		case re::Texture::Texture2D:
		{
			SEAssert(texView.Texture2D.m_planeSlice == 0, "TODO: Support multi plane formats here");

			SEAssert(texView.Texture2D.m_firstMip < numMips &&
				(texView.Texture2D.m_mipLevels == re::Texture::k_allMips ||
					texView.Texture2D.m_firstMip + texView.Texture2D.m_mipLevels <= numMips),
				"Indexes are out of bounds");

			totalSubresources = texView.Texture2D.m_mipLevels;
			if (texView.Texture2D.m_mipLevels == re::Texture::k_allMips)
			{
				totalSubresources = numMips - texView.Texture2D.m_firstMip;
			}
			subresourceIndexes.reserve(totalSubresources);

			for (uint32_t relMipIdx = 0; relMipIdx < totalSubresources; ++relMipIdx)
			{
				subresourceIndexes.emplace_back(
					re::TextureView::GetSubresourceIndexFromRelativeOffsets(texture, texView, 0, relMipIdx));
			}
		}
		break;
		case re::Texture::Texture2DArray:
		{
			SEAssert(texView.Texture2DArray.m_planeSlice == 0, "TODO: Support multi plane formats here");

			SEAssert(texView.Texture2DArray.m_firstMip < numMips &&
				(texView.Texture2DArray.m_mipLevels == re::Texture::k_allMips ||
					texView.Texture2DArray.m_firstMip + texView.Texture2DArray.m_mipLevels <= numMips) &&
				texView.Texture2DArray.m_arraySize > 0 &&
				((texView.Texture2DArray.m_firstArraySlice < texParams.m_arraySize &&
					texView.Texture2DArray.m_firstArraySlice + texView.Texture2DArray.m_arraySize <= texParams.m_arraySize) ||
					(texParams.m_dimension == re::Texture::TextureCube ||
						texParams.m_dimension == re::Texture::TextureCubeArray) &&
					(texView.Texture2DArray.m_firstArraySlice < (texParams.m_arraySize * 6) &&
						texView.Texture2DArray.m_firstArraySlice + texView.Texture2DArray.m_arraySize <= (texParams.m_arraySize * 6))),
				"Indexes are out of bounds");

			uint32_t totalMips = texView.Texture2DArray.m_mipLevels;
			if (texView.Texture2DArray.m_mipLevels == re::Texture::k_allMips)
			{
				totalMips = numMips - texView.Texture2DArray.m_firstMip;
			}

			totalSubresources = texView.Texture2DArray.m_arraySize * totalMips;

			subresourceIndexes.reserve(totalSubresources);

			for (uint32_t relArrayIdx = 0; relArrayIdx < texView.Texture2DArray.m_arraySize; ++relArrayIdx)
			{
				for (uint32_t relMipIdx = 0; relMipIdx < totalMips; ++relMipIdx)
				{
					subresourceIndexes.emplace_back(
						re::TextureView::GetSubresourceIndexFromRelativeOffsets(texture, texView, relArrayIdx, relMipIdx));
				}
			}
		}
		break;
		case re::Texture::Texture3D:
		{
			SEAssert(texView.Texture3D.m_firstMip < numMips &&
				(texView.Texture3D.m_mipLevels == re::Texture::k_allMips ||
					texView.Texture3D.m_firstMip + texView.Texture3D.m_mipLevels <= numMips) &&
				texView.Texture3D.m_firstWSlice < texParams.m_arraySize &&
				(texView.Texture3D.m_wSize == re::Texture::k_allArrayElements ||
					texView.Texture3D.m_firstWSlice + texView.Texture3D.m_wSize <= texParams.m_arraySize),
				"Indexes are out of bounds");

			totalSubresources = texView.Texture3D.m_mipLevels;
			if (texView.Texture3D.m_mipLevels == re::Texture::k_allMips)
			{
				totalSubresources = numMips - texView.Texture3D.m_firstMip;
			}
			subresourceIndexes.reserve(totalSubresources);

			for (uint32_t relMipIdx = 0; relMipIdx < totalSubresources; ++relMipIdx)
			{
				subresourceIndexes.emplace_back(
					re::TextureView::GetSubresourceIndexFromRelativeOffsets(texture, texView, 0, relMipIdx));
			}
		}
		break;
		case re::Texture::TextureCube:
		{
			SEAssert(texView.TextureCube.m_firstMip < numMips &&
				(texView.TextureCube.m_mipLevels == re::Texture::k_allMips ||
					texView.TextureCube.m_firstMip + texView.TextureCube.m_mipLevels <= numMips),
				"Indexes are out of bounds");

			uint32_t mipLevels = texView.TextureCube.m_mipLevels;
			totalSubresources = texView.TextureCube.m_mipLevels * 6;
			if (texView.TextureCube.m_mipLevels == re::Texture::k_allMips)
			{
				totalSubresources = (numMips - texView.TextureCube.m_firstMip) * 6;
				mipLevels = numMips;
			}
			subresourceIndexes.reserve(totalSubresources);

			for (uint32_t faceIdx = 0; faceIdx < 6; ++faceIdx)
			{
				for (uint32_t mipIdx = 0; mipIdx < mipLevels; ++mipIdx)
				{
					subresourceIndexes.emplace_back(
						(faceIdx * numMips) + (texView.TextureCube.m_firstMip + mipIdx));
				}
			}
		}
		break;
		case re::Texture::TextureCubeArray:
		{
			SEAssert(texView.TextureCubeArray.m_firstMip < numMips &&
				(texView.TextureCubeArray.m_mipLevels == re::Texture::k_allMips ||
					texView.TextureCubeArray.m_firstMip + texView.TextureCubeArray.m_mipLevels <= numMips) &&
				texView.TextureCubeArray.m_first2DArrayFace < (texParams.m_arraySize * 6) &&
				texView.TextureCubeArray.m_first2DArrayFace + texView.TextureCubeArray.m_numCubes * 6 <= (texParams.m_arraySize * 6),
				"Indexes are out of bounds");

			uint32_t totalMips = texView.TextureCubeArray.m_mipLevels;
			if (texView.TextureCubeArray.m_mipLevels == re::Texture::k_allMips)
			{
				totalMips = numMips - texView.TextureCubeArray.m_firstMip;
			}

			uint32_t totalFaces = texView.TextureCubeArray.m_numCubes * 6;

			totalSubresources = totalFaces * totalMips;

			subresourceIndexes.reserve(totalSubresources);

			const uint32_t firstArrayIdx = texView.TextureCubeArray.m_first2DArrayFace / 6;

			for (uint32_t arrayIdx = 0; arrayIdx < texView.TextureCubeArray.m_numCubes; ++arrayIdx)
			{
				for (uint32_t faceIdx = 0; faceIdx < 6; faceIdx++)
				{
					for (uint32_t mipIdx = 0; mipIdx < totalMips; ++mipIdx)
					{
						subresourceIndexes.emplace_back(texture->GetSubresourceIndex(
							firstArrayIdx + arrayIdx,
							faceIdx,
							texView.TextureCubeArray.m_firstMip + mipIdx));
					}
				}
			}
		}
		break;
		default: SEAssertF("Invalid dimension");
		}

		SEAssert(!subresourceIndexes.empty() &&
			subresourceIndexes.size() == totalSubresources,
			"Miscalculated the number of resources to transition");

		return subresourceIndexes;
	}


	TextureView TextureView::CreateDefaultView(re::Texture const& tex)
	{
		re::Texture::TextureParams const& texParams = tex.GetTextureParams();

		switch (texParams.m_dimension)
		{
		case re::Texture::Dimension::Texture1D: return TextureView(
			TextureView::Texture1DView(0, re::Texture::k_allMips, 0.f));
		case re::Texture::Dimension::Texture1DArray: return TextureView(
			TextureView::Texture1DArrayView(0, re::Texture::k_allMips, 0, texParams.m_arraySize, 0.f));
		case re::Texture::Dimension::Texture2D: return TextureView(
			TextureView::Texture2DView(0, re::Texture::k_allMips, 0, 0.f));
		case re::Texture::Dimension::Texture2DArray: return TextureView(
			TextureView::Texture2DArrayView(0, re::Texture::k_allMips, 0, texParams.m_arraySize, 0, 0.f));
		case re::Texture::Dimension::Texture3D: return TextureView(
			TextureView::Texture3DView(0, re::Texture::k_allMips, 0.f, 0, texParams.m_arraySize));
		case re::Texture::Dimension::TextureCube: return TextureView(
			TextureView::TextureCubeView(0, re::Texture::k_allMips, 0.f));
		case re::Texture::Dimension::TextureCubeArray: return TextureView(
			TextureView::TextureCubeArrayView(0, re::Texture::k_allMips, 0, texParams.m_arraySize, 0.f));
		default: SEAssertF("Invalid dimension");
		}
		return TextureView(TextureView::Texture2DView()); // This should never happen
	}


	// -----------------------------------------------------------------------------------------------------------------


	TextureAndSamplerInput::TextureAndSamplerInput(
		char const* shaderName,
		re::Texture const* texture,
		re::Sampler const* sampler,
		TextureView const& texView)
		: m_shaderName(shaderName)
		, m_texture(texture)
		, m_sampler(sampler)
		, m_textureView(texView)
	{
	}


	TextureAndSamplerInput::TextureAndSamplerInput(
		std::string const& shaderName,
		re::Texture const* texture,
		re::Sampler const* sampler,
		TextureView const& texView)
		: TextureAndSamplerInput(shaderName.c_str(), texture, sampler, texView)
	{
	}


	TextureAndSamplerInput::TextureAndSamplerInput(TextureAndSamplerInput const& rhs) noexcept
	{
		*this = rhs;
	}


	TextureAndSamplerInput::TextureAndSamplerInput(TextureAndSamplerInput&& rhs) noexcept
	{
		*this = std::move(rhs);
	}


	TextureAndSamplerInput& TextureAndSamplerInput::operator=(TextureAndSamplerInput const& rhs) noexcept
	{
		if (&rhs != this)
		{
			m_shaderName = rhs.m_shaderName;
			m_texture = rhs.m_texture;
			m_sampler = rhs.m_sampler;
			m_textureView = rhs.m_textureView;
		}
		return *this;
	}


	TextureAndSamplerInput& TextureAndSamplerInput::operator=(TextureAndSamplerInput&& rhs) noexcept
	{
		if (&rhs != this)
		{
			m_shaderName = std::move(rhs.m_shaderName);
			m_texture = rhs.m_texture;
			m_sampler = rhs.m_sampler;
			m_textureView = rhs.m_textureView;

		}
		return *this;
	}
}