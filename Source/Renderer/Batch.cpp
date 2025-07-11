// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
#include "Buffer.h"
#include "Texture.h"
#include "TextureView.h"
#include "VertexStream.h"

#include "Core/Assert.h"


namespace gr
{
	Batch::Batch()
		: m_type(BatchType::Invalid)
		, m_drawStyleBitmask(0)
		, m_batchFilterBitmask(0)
	{
		// We must zero-initialize our InvPtrs to ensure they don't contain garbage before initializing RasterParams
		memset(&m_rasterParams.m_vertexBuffers, 0, sizeof(m_rasterParams.m_vertexBuffers));
		memset(&m_rasterParams.m_indexBuffer, 0, sizeof(m_rasterParams.m_indexBuffer));

		// Note: We don't actually initialize a union member here, as we don't know which one will be used. But we need
		// to ensure that the union is zero-initialized to avoid garbage values in the InvPtrs etc
	}


	Batch::Batch(BatchType batchType)
		: m_type(batchType)
		, m_drawStyleBitmask(0)
		, m_batchFilterBitmask(0)
	{
		switch (m_type)
		{
		case BatchType::Raster:
		{
			// We must zero-initialize our InvPtrs to ensure they don't contain garbage before initializing RasterParams
			memset(&m_rasterParams.m_vertexBuffers, 0, sizeof(m_rasterParams.m_vertexBuffers));
			memset(&m_rasterParams.m_indexBuffer, 0, sizeof(m_rasterParams.m_indexBuffer));

			m_rasterParams = {};
		}
		break;
		case BatchType::Compute:
		{
			m_computeParams = {};
		}
		break;
		case BatchType::RayTracing:
		{
			// Zero-initialize to ensure shared_ptr doesn't contain garbage
			memset(&m_rayTracingParams, 0, sizeof(m_rayTracingParams));

			m_rayTracingParams = {};
		}
		break;
		default: SEAssertF("Invalid type");
		}
	};


	Batch::Batch(Batch&& rhs) noexcept
	{
		switch (rhs.m_type)
		{
		case BatchType::Raster:
		{
			// We must zero-initialize our InvPtrs to ensure they don't contain garbage before initializing RasterParams
			memset(&m_rasterParams.m_vertexBuffers, 0, sizeof(m_rasterParams.m_vertexBuffers));
			memset(&m_rasterParams.m_indexBuffer, 0, sizeof(m_rasterParams.m_indexBuffer));
		}
		break;
		case BatchType::Compute:
		{
			// Zero-initialize for consistency
			memset(&m_computeParams, 0, sizeof(m_computeParams));
		}
		break;
		case BatchType::RayTracing:
		{
			// Zero-initialize to ensure shared_ptr doesn't contain garbage
			memset(&m_rayTracingParams, 0, sizeof(m_rayTracingParams));
		}
		break;
		default: SEAssertF("Invalid type");
		}

		*this = std::move(rhs);
	};


	Batch& Batch::operator=(Batch&& rhs) noexcept
	{
		if (this != &rhs)
		{
			m_type = rhs.m_type;

			switch (m_type)
			{
			case BatchType::Raster:
			{
				m_rasterParams = std::move(rhs.m_rasterParams);
			}
			break;
			case BatchType::Compute:
			{
				m_computeParams = std::move(rhs.m_computeParams);
			}
			break;
			case BatchType::RayTracing:
			{
				m_rayTracingParams = std::move(rhs.m_rayTracingParams);
			}
			break;
			default: SEAssertF("Invalid type");
			}

			m_effectID = std::move(rhs.m_effectID);

			m_drawStyleBitmask = rhs.m_drawStyleBitmask;
			rhs.m_drawStyleBitmask = 0;

			m_batchFilterBitmask = rhs.m_batchFilterBitmask;
			rhs.m_batchFilterBitmask = 0;

			m_batchBuffers = std::move(rhs.m_batchBuffers);

			m_batchTextureSamplerInputs = std::move(rhs.m_batchTextureSamplerInputs);
			m_batchRWTextureInputs = std::move(rhs.m_batchRWTextureInputs);

			m_batchRootConstants = std::move(rhs.m_batchRootConstants);

			SetDataHash(rhs.GetDataHash());
			rhs.ResetDataHash();
		}
		return *this;
	};


	Batch::Batch(Batch const& rhs) noexcept
	{
		switch (rhs.m_type)
		{
		case BatchType::Raster:
		{
			// We must zero-initialize our InvPtrs to ensure they don't contain garbage before initializing RasterParams
			memset(&m_rasterParams.m_vertexBuffers, 0, sizeof(m_rasterParams.m_vertexBuffers));
			memset(&m_rasterParams.m_indexBuffer, 0, sizeof(m_rasterParams.m_indexBuffer));
		}
		break;
		case BatchType::Compute:
		{
			// Zero-initialize for consistency
			memset(&m_computeParams, 0, sizeof(m_computeParams));
		}
		break;
		case BatchType::RayTracing:
		{
			// Zero-initialize to ensure shared_ptr doesn't contain garbage
			memset(&m_rayTracingParams, 0, sizeof(m_rayTracingParams));
		}
		break;
		default: SEAssertF("Invalid type");
		}

		*this = rhs;
	};


	Batch& Batch::operator=(Batch const& rhs) noexcept
	{
		if (this != &rhs)
		{
			m_type = rhs.m_type;

			switch (m_type)
			{
			case BatchType::Raster:
			{
				m_rasterParams = rhs.m_rasterParams;
			}
			break;
			case BatchType::Compute:
			{
				m_computeParams = rhs.m_computeParams;
			}
			break;
			case BatchType::RayTracing:
			{
				m_rayTracingParams = rhs.m_rayTracingParams;
			}
			break;
			default: SEAssertF("Invalid type");
			}

			m_effectID = rhs.m_effectID;
			m_drawStyleBitmask = rhs.m_drawStyleBitmask;
			m_batchFilterBitmask = rhs.m_batchFilterBitmask;

			m_batchBuffers = rhs.m_batchBuffers;

			m_batchTextureSamplerInputs = rhs.m_batchTextureSamplerInputs;
			m_batchRWTextureInputs = rhs.m_batchRWTextureInputs;

			m_batchRootConstants = rhs.m_batchRootConstants;

			SetDataHash(rhs.GetDataHash());
		}
		return *this;
	};


	Batch::~Batch()
	{
		switch (m_type)
		{
		case BatchType::Raster:
		{
			m_rasterParams.~RasterParams();
		}
		break;
		case BatchType::Compute:
		{
			m_computeParams.~ComputeParams();
		}
		break;
		case BatchType::RayTracing:
		{
			m_rayTracingParams.~RayTracingParams();
		}
		break;
		case BatchType::Invalid:
		{
			// Do nothing
		}
		break;
		default: SEAssertF("Invalid type");
		}
		
		// We'll let everything else be destroyed when it goes out of scope
	};


	void Batch::Destroy()
	{
		// This function is effectively an in-place destructor for Batches held by the BatchPool. We zero out the unions
		// as a precaution, as the memory might be reused
		switch (m_type)
		{
		case BatchType::Raster:
		{
			m_rasterParams.~RasterParams();

			memset(&m_rasterParams.m_vertexBuffers, 0, sizeof(m_rasterParams.m_vertexBuffers));
			memset(&m_rasterParams.m_indexBuffer, 0, sizeof(m_rasterParams.m_indexBuffer));
		}
		break;
		case BatchType::Compute:
		{
			m_computeParams.~ComputeParams();

			memset(&m_computeParams, 0, sizeof(m_computeParams));
		}
		break;
		case BatchType::RayTracing:
		{
			m_rayTracingParams.~RayTracingParams();

			memset(&m_rayTracingParams, 0, sizeof(m_rayTracingParams));
		}
		break;
		default: SEAssertF("Invalid type: Was Batch already destroyed?");
		}
		m_type = BatchType::Invalid;

		m_effectID = 0;
		m_drawStyleBitmask = 0;
		m_batchFilterBitmask = 0;
		
		m_batchBuffers.clear();

		m_batchTextureSamplerInputs.clear();
		m_batchRWTextureInputs.clear();
		m_batchRootConstants.Destroy();

		ResetDataHash();
	};


	void Batch::ComputeDataHash()
	{		
		SEAssert(GetDataHash() == 0, "Data hash already computed. This is unexpected");

		// Note: We don't consider the re::Lifetime m_lifetime, as we want single frame/permanent batches to instance

		AddDataBytesToHash(m_type);

		switch (m_type)
		{
		case BatchType::Raster:
		{
			// Note: We assume the hash is used to evaluate batch equivalence when sorting, to enable instancing. Thus,
			// we don't consider the m_batchGeometryMode or m_numInstances

			AddDataBytesToHash(m_rasterParams.m_primitiveTopology);

			for (re::VertexBufferInput const& vertexStream : m_rasterParams.m_vertexBuffers)
			{
				if (vertexStream.GetStream() == nullptr)
				{
					break;
				}

				AddDataBytesToHash(vertexStream.GetStream()->GetDataHash());
			}
			if (m_rasterParams.m_indexBuffer.GetStream())
			{
				AddDataBytesToHash(m_rasterParams.m_indexBuffer.GetStream()->GetDataHash());
			}

			if (m_rasterParams.m_vertexStreamOverrides)
			{
				// Hash the pointer to differentiate batches with overrides (e.g. animated verts) of otherwise seemingly
				// identical vertex stream data
				AddDataBytesToHash(m_rasterParams.m_vertexStreamOverrides);
			}

			AddDataBytesToHash(m_rasterParams.m_materialUniqueID);

			SEStaticAssert(sizeof(Batch::RasterParams) == 976, "Must update this if RasterParams size has changed");
		}
		break;
		case BatchType::Compute:
		{
			// Instancing doesn't apply to compute shaders; m_threadGroupCount is included just as it's a differentiator
			AddDataBytesToHash(m_computeParams.m_threadGroupCount);

			SEStaticAssert(sizeof(Batch::ComputeParams) == 12, "Must update this if ComputeParams size has changed");
		}
		break;
		case BatchType::RayTracing:
		{
			SEStaticAssert(sizeof(Batch::RayTracingParams) == 80 || sizeof(Batch::RayTracingParams) == 72,
				"Must update this if RayTracingParams debug/release size has changed");

			AddDataBytesToHash(m_rayTracingParams.m_operation);
			AddDataBytesToHash(m_rayTracingParams.m_ASInput.m_shaderName);
			AddDataBytesToHash(m_rayTracingParams.m_ASInput.m_accelerationStructure->GetNameHash());
			AddDataBytesToHash(m_rayTracingParams.m_dispatchDimensions);
		}
		break;
		default: SEAssertF("Invalid type");
		}

		// Shader:
		AddDataBytesToHash(m_effectID);
		AddDataBytesToHash(m_drawStyleBitmask);
		AddDataBytesToHash(m_batchFilterBitmask);
		
		// Note: We must consider buffers added before instancing has been calcualted, as they allow us to
		// differentiate batches that are otherwise identical. We'll use the same, identical buffer on the merged
		// instanced batches later
		for (size_t i = 0; i < m_batchBuffers.size(); i++)
		{
			AddDataBytesToHash(m_batchBuffers[i].GetBuffer()->GetUniqueID());
		}

		// Include textures/samplers in the batch hash:
		for (auto const& texSamplerInput : m_batchTextureSamplerInputs)
		{
			AddDataBytesToHash(texSamplerInput.m_texture->GetUniqueID());
			AddDataBytesToHash(texSamplerInput.m_sampler->GetUniqueID());
		}

		// Include RW textures in the batch hash:
		for (auto const& rwTexInput : m_batchRWTextureInputs)
		{
			AddDataBytesToHash(rwTexInput.m_texture->GetUniqueID());
		}

		// Root constants:
		AddDataBytesToHash(m_batchRootConstants.GetDataHash());
	}


	void Batch::SetFilterMaskBit(Filter filterBit, bool enabled)
	{
		if (enabled)
		{
			m_batchFilterBitmask |= static_cast<gr::Batch::FilterBitmask>(filterBit);
		}
		else if (m_batchFilterBitmask & static_cast<gr::Batch::FilterBitmask>(filterBit))
		{
			m_batchFilterBitmask ^= static_cast<gr::Batch::FilterBitmask>(filterBit);
		}
	}


	bool Batch::MatchesFilterBits(gr::Batch::FilterBitmask required, gr::Batch::FilterBitmask excluded) const
	{
		if (required == 0 && excluded == 0) // Accept all batches by default
		{
			return true;
		}

		// Only a single bit on a Batch must match with the excluded mask for a Batch to be excluded
		const bool isExcluded = (m_batchFilterBitmask & excluded);

		// A Batch must contain all bits in the included mask to be included
		// A Batch may contain more bits than what is required, so long as it matches all required bits
		bool isFullyIncluded = false;
		if (!isExcluded)
		{
			const gr::Batch::FilterBitmask invertedRequiredBits = ~required;
			const gr::Batch::FilterBitmask matchingBatchBits = (m_batchFilterBitmask & invertedRequiredBits) ^ m_batchFilterBitmask;
			isFullyIncluded = (matchingBatchBits == required);
		}

		return !isExcluded && isFullyIncluded;
	}


	void Batch::SetBuffer(std::string_view shaderName, std::shared_ptr<re::Buffer> const& buffer)
	{
		SetBuffer(re::BufferInput(shaderName, buffer));
	}


	void Batch::SetBuffer(std::string_view shaderName, std::shared_ptr<re::Buffer> const& buffer, re::BufferView const& view)
	{
		SetBuffer(re::BufferInput(shaderName, buffer, view));
	}


	void Batch::SetBuffer(re::BufferInput&& bufferInput)
	{
		SEAssert(!bufferInput.GetName().empty() &&
			bufferInput.GetBuffer() != nullptr,
			"Cannot set a unnamed or null buffer");

		SEAssert(bufferInput.GetLifetime() != re::Lifetime::SingleFrame,
			"Single frame buffers cannot be set directly on a batch");

#if defined(_DEBUG)
		for (auto const& existingBuffer : m_batchBuffers)
		{
			SEAssert(bufferInput.GetBuffer()->GetNameHash() != existingBuffer.GetBuffer()->GetNameHash(),
				"Buffer with the same name has already been set. Re-adding it changes the data hash");
		}
#endif
		if (m_batchBuffers.empty())
		{
			m_batchBuffers.reserve(k_batchBufferIDsReserveAmount);
		}

		m_batchBuffers.emplace_back(std::move(bufferInput));
	}


	void Batch::SetBuffer(re::BufferInput const& bufferInput)
	{
		SetBuffer(re::BufferInput(bufferInput));
	}


	void Batch::SetTextureInput(
		std::string_view shaderName,
		core::InvPtr<re::Texture> const& texture,
		core::InvPtr<re::Sampler> const& sampler,
		re::TextureView const& texView)
	{
		SEAssert(!shaderName.empty(), "Invalid shader sampler name");
		SEAssert(texture != nullptr, "Invalid texture");
		SEAssert(sampler.IsValid(), "Invalid sampler");
		SEAssert(texView.m_viewDimension != re::Texture::Dimension_Invalid, "Invalid view dimension");
		SEAssert((texture->GetTextureParams().m_usage & re::Texture::Usage::ColorSrc) != 0, "Invalid usage");

#if defined(_DEBUG)
		for (auto const& existingTexAndSamplerInput : m_batchTextureSamplerInputs)
		{
			SEAssert(existingTexAndSamplerInput.m_texture != texture ||
				strcmp(existingTexAndSamplerInput.m_shaderName.c_str(), shaderName.data()) != 0,
				"This Texture has already been added with the same shader name. Re-adding it changes the data hash");
		}
#endif

		if (m_batchTextureSamplerInputs.empty())
		{
			m_batchTextureSamplerInputs.reserve(k_texSamplerInputReserveAmount);
		}

		m_batchTextureSamplerInputs.emplace_back(shaderName, texture, sampler, texView);
	}


	void Batch::SetRWTextureInput(
		std::string_view shaderName,
		core::InvPtr<re::Texture> const& texture,
		re::TextureView const& texView)
	{
		SEAssert(!shaderName.empty(), "Invalid shader sampler name");
		SEAssert(texture != nullptr, "Invalid texture");
		SEAssert(texView.m_viewDimension != re::Texture::Dimension_Invalid, "Invalid view dimension");
		SEAssert((texture->GetTextureParams().m_usage & re::Texture::Usage::ColorSrc) != 0 && 
			(texture->GetTextureParams().m_usage & re::Texture::Usage::ColorTarget) != 0,
			"Invalid usage");

#if defined(_DEBUG)
		for (auto const& existingTexAndSamplerInput : m_batchRWTextureInputs)
		{
			SEAssert(existingTexAndSamplerInput.m_texture != texture ||
				strcmp(existingTexAndSamplerInput.m_shaderName.c_str(), shaderName.data()) != 0,
				"This Texture has already been added with the same shader name. Re-adding it changes the data hash");
		}
#endif

		if (m_batchRWTextureInputs.empty())
		{
			m_batchRWTextureInputs.reserve(k_rwTextureInputReserveAmount);
		}

		m_batchRWTextureInputs.emplace_back(re::RWTextureInput{ shaderName, texture, texView });
	}
}