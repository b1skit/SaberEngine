// © 2024 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "HeapManager_DX12.h"
#include "SysInfo_DX12.h"

#include "Core/Util/MathUtils.h"

#include <d3dx12.h>


// Enable this to track the resources that have registered names
//#define DEBUG_MAP_RESOURCE_NAMES

// Enable this to sanity-check pages
//#define ENABLE_RESOURCE_PAGE_VALIDATION


namespace
{
#if defined DEBUG_MAP_RESOURCE_NAMES
	static std::unordered_set<std::wstring> s_registeredResourceNames;
	static std::mutex s_registeredResourceNamesMutex;
#endif


	uint32_t GetNumberOfSubresources(D3D12_RESOURCE_DESC const& resourceDesc)
	{
		if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
		{
			return resourceDesc.MipLevels;
		}
		return resourceDesc.MipLevels * resourceDesc.DepthOrArraySize;
	}


	// Note: Returns 0 for unsupported/unexpected formats. Assert on the return value to keep this constexpr
	constexpr uint8_t DXGIFormatToBitsPerPixel(DXGI_FORMAT format)
	{
		switch (format)
		{
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
			return 128;
		case DXGI_FORMAT_R32G32B32_TYPELESS:
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT:
			return 96;
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT:
			// 64
		case DXGI_FORMAT_R32G32_TYPELESS:
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT:
			// 64
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return 64;
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UINT:
		case DXGI_FORMAT_R11G11B10_FLOAT:
			// 32
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_SINT:
			// 32
		case DXGI_FORMAT_R16G16_TYPELESS:
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R16G16_SINT:
			// 32
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
			// 32
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return 32;
		case DXGI_FORMAT_R8G8_TYPELESS:
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R8G8_SINT:
			// 16
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R16_SINT:
			return 16;
		case DXGI_FORMAT_R8_TYPELESS:
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT:
		case DXGI_FORMAT_A8_UNORM:
			return 8;
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
			return 4;
		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
			// 8
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
			return 8;
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
			return 4;
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
			// 8
		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
			// 8
		case DXGI_FORMAT_BC7_TYPELESS:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return 8;
		default:
			return 0; // Unexpected/unsupported format: Catch this with an assert when returning to keep this constexpr
		}
	}


	constexpr bool IsCompressedFormat(DXGI_FORMAT format)
	{
		switch (format)
		{
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC7_TYPELESS:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return true;
		default:
			return false;
		}
	}


	void GetCompressedTileDimensions(DXGI_FORMAT format, uint32_t& width, uint32_t& height)
	{
		switch (format)
		{
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
		{
			width = 512;
			height = 256;
		}
		break;
		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC7_TYPELESS:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
		{
			width = 256;
			height = 256;
		}
		break;
		default: SEAssertF("Invalid format");
		}
	}


	void GetUncompressedTileDimensions(uint8_t bitsPerPixel, uint32_t& width, uint32_t& height)
	{
		// https://learn.microsoft.com/en-us/windows/win32/direct3d11/texture2d-and-texture2darray-subresource-tiling
		switch (bitsPerPixel)
		{
		case 8:
		{
			width = 256;
			height = 256;
		}
		break;
		case 16:
		{
			width = 256;
			height = 128;
		}
		break;
		case 32:
		{
			width = 128;
			height = 128;
		}
		break;
		case 64:
		{
			width = 128;
			height = 64;
		}
		break;
		case 128:
		{
			width = 64;
			height = 64;
		}
		break;
		default: SEAssertF("Invalid bits per pixel");
		}
	}


	bool SmallAlignmentSupported(dx12::ResourceDesc const& resourceDesc)
	{
		// https://asawicki.info/news_1726_secrets_of_direct3d_12_resource_alignment

		D3D12_RESOURCE_DESC const& d3dResourceDesc = resourceDesc.m_resourceDesc;

		if (d3dResourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		{
			return false;
		}

		if (resourceDesc.m_resourceDesc.Layout != D3D12_TEXTURE_LAYOUT_UNKNOWN)
		{
			return false;
		}

		constexpr D3D12_RESOURCE_FLAGS k_isRenderTarget =
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		if (d3dResourceDesc.Flags & k_isRenderTarget)
		{
			return false;
		}

		uint32_t bitsPerPixel = DXGIFormatToBitsPerPixel(d3dResourceDesc.Format);
		SEAssert(bitsPerPixel > 0,
			"DXGIFormatToBitsPerPixel returned 0. This suggests the resource has an unsupported/unexpected format");

		// Get the tile dimensions for the format:
		uint32_t tileWidth = 0;
		uint32_t tileHeight = 0;
		const bool isCompressedFormat = IsCompressedFormat(d3dResourceDesc.Format);
		if (isCompressedFormat)
		{
			GetCompressedTileDimensions(d3dResourceDesc.Format, tileWidth, tileHeight);
		}
		else
		{
			GetUncompressedTileDimensions(bitsPerPixel, tileWidth, tileHeight);
		}

		if (d3dResourceDesc.SampleDesc.Count > 1)
		{
			SEAssert(resourceDesc.m_isMSAATexture,
				"D3D resource description specifies multiple samples, but the MSAA flag is not set");

			SEAssert(d3dResourceDesc.SampleDesc.Count == 2 || d3dResourceDesc.SampleDesc.Count == 4 ||
				d3dResourceDesc.SampleDesc.Count == 8 || d3dResourceDesc.SampleDesc.Count == 16,
				"Unexpected multisample count");

			switch (d3dResourceDesc.SampleDesc.Count)
			{
			case 2:
			{
				tileWidth /= 2;
			}
			break;
			case 4:
			{
				tileWidth /= 2;
				tileHeight /= 2;
			}
			break;
			case 8:
			{
				tileWidth /= 4;
				tileHeight /= 2;
			}
			break;
			case 16:
			{
				tileWidth /= 4;
				tileHeight /= 4;
			}
			break;
			default: SEAssertF("Unexpected multisample count");
			}
		}

		// The runtime will assume near-equilateral tile shapes of 4KB, and calculate the number of tiles needed for the
		// most-detailed mip level. If the number of tiles is equal to or less than 16, then the application can create
		// a 4KB aligned resource
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_resource_desc#alignment

		const uint32_t width = util::CheckedCast<uint32_t>(d3dResourceDesc.Width);
		const uint32_t height = d3dResourceDesc.Height;

		const uint32_t numTiles = util::DivideAndRoundUp(width, tileWidth) * util::DivideAndRoundUp(height, tileHeight);

		if (numTiles > 16)
		{
			return false;
		}

		// Note: We don't consider the array depth here:
		// For arrayed surfaces, the set of packed mips and the number of packed tiles storing those mips applies
		// individually for each array slice
		//https://learn.microsoft.com/en-us/windows/win32/direct3d11/mipmap-packing

		return true;
	}


	void GetResourceSizeAndAlignment(
		ID3D12Device2* device,
		dx12::ResourceDesc const& resourceDesc,
		uint32_t& numBytesOut,
		uint32_t& alignmentOut)
	{
		D3D12_RESOURCE_DESC const& d3dResourceDesc = resourceDesc.m_resourceDesc;
		
		// Check if we can use small alignment:
		if (SmallAlignmentSupported(resourceDesc))
		{
			SEAssert(d3dResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D,
				"Only Texture2D/Texture2D arrays are supported for small alignment");

			// GetResourceAllocationInfo() will return the "larger" size requirement unless we specifically ask for the
			// small-aligned version
			alignmentOut = resourceDesc.m_isMSAATexture ?
				D3D12_SMALL_MSAA_RESOURCE_PLACEMENT_ALIGNMENT : D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT;

			D3D12_RESOURCE_DESC smallResourceDesc = d3dResourceDesc;
			smallResourceDesc.Alignment = alignmentOut;

			D3D12_RESOURCE_ALLOCATION_INFO const& smallAllocationInfo = 
				device->GetResourceAllocationInfo(0, 1, &smallResourceDesc);

			numBytesOut = util::CheckedCast<uint32_t>(smallAllocationInfo.SizeInBytes);
		}
		else
		{
			D3D12_RESOURCE_ALLOCATION_INFO const& allocationInfo = device->GetResourceAllocationInfo(0, 1, &d3dResourceDesc);

			numBytesOut = util::CheckedCast<uint32_t>(allocationInfo.SizeInBytes);
			alignmentOut = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT; // In D3D12, all buffers have 64KB alignment
		}
	}


	uint32_t ResourceDescToHeapAlignment(dx12::ResourceDesc const& resourceDesc)
	{
		// DX12 heaps have a default of 64KB alignment, or 4MB if the heap contains MSAA textures.
		// Note: This is seperate to the alignment of the resources placed into a heap
		if (resourceDesc.m_isMSAATexture)
		{
			return D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
		}
		return D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	}


	DataHash ComputePagedResourceHeapHash(
		dx12::ResourceDesc const& resourceDesc, uint32_t heapAlignment, bool canMixResourceTypes)
	{
		DataHash resourceHeapKey = 0;
		util::AddDataBytesToHash(resourceHeapKey, resourceDesc.m_heapType);
		util::AddDataBytesToHash(resourceHeapKey, resourceDesc.m_isMSAATexture);

		util::AddDataBytesToHash(resourceHeapKey, heapAlignment);

		if (!canMixResourceTypes)
		{
			// Heap tier 1 must keep buffers, non-render/depth target textures, and render/depth target textures in
			// seperate heaps. Heap tier 2 can mix all 3 together in the same heap
			// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_heap_tier#remarks
			switch (resourceDesc.m_resourceDesc.Dimension)
			{
			case D3D12_RESOURCE_DIMENSION_BUFFER:
			{
				util::AddDataBytesToHash(resourceHeapKey, resourceDesc.m_resourceDesc.Dimension);
			}
			break;
			case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			{
				// If we're storing textures, we need to differentiate them between non-RT/RT textures:
				constexpr D3D12_RESOURCE_FLAGS k_renderDepthStencilFlags =
					D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

				const bool isTargetType = (resourceDesc.m_resourceDesc.Flags & k_renderDepthStencilFlags) != 0;
				util::AddDataBytesToHash(resourceHeapKey, isTargetType);
			}
			break;
			case D3D12_RESOURCE_DIMENSION_UNKNOWN:
			default: SEAssertF("Invalid resource dimension");
				break;
			}
		}

		return resourceHeapKey;
	}


	void ValidateHeapConfig(dx12::HeapDesc const& heapDesc, uint32_t alignment)
	{
#if defined(_DEBUG)
		SEAssert(!heapDesc.m_allowMSAATextures || 
			(heapDesc.m_heapFlags & D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES) == 0 ||
			(heapDesc.m_heapFlags & D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES) == 0,
			"Flags are incompatible with the MSAA flag: We can't allow MSAA if no textures are allowed");

		SEAssert(glm::fmod(glm::log2(static_cast<float>(alignment)), 1.f) == 0,
			"Alignment must be a power-of-two, and the page size must be perfectly divisible by it");

		SEAssert(!heapDesc.m_allowMSAATextures || 
			heapDesc.m_heapType == D3D12_HEAP_TYPE_DEFAULT,
			"Trying to allocate a heap that supports MSAA textures in a non-default heap type. This is unexpected.");
#endif
	}


	void ValidateResourceDesc(dx12::ResourceDesc const& resourceDesc)
	{
#if defined(_DEBUG)
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_flags
		SEAssert(!resourceDesc.m_isMSAATexture ||
			((resourceDesc.m_resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0 &&
				((resourceDesc.m_resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) == 0)),
			"Flags are incompatible with the MSAA");

		static const bool supportsMSAA = 
			dx12::SysInfo::GetMaxMultisampleQualityLevel(resourceDesc.m_resourceDesc.Format) > 0;

		SEAssert(!resourceDesc.m_isMSAATexture ||
			(resourceDesc.m_resourceDesc.SampleDesc.Count > 0 &&
				supportsMSAA),
			"Resource is misconfigured, or MSAA is not supported");
#endif
	}
}


namespace dx12
{
	HeapAllocation::HeapAllocation()
		: m_owningHeapPage(nullptr)
		, m_heap(nullptr)
		, m_baseOffset(0)
		, m_numBytes(0)
	{
	}


	HeapAllocation::HeapAllocation(
		HeapPage* owningPage, ID3D12Heap* heap, uint32_t baseOffset, uint32_t numBytes)
		: m_owningHeapPage(owningPage)
		, m_heap(heap)
		, m_baseOffset(baseOffset)
		, m_numBytes(numBytes)
	{
		SEAssert(m_owningHeapPage != nullptr && m_heap != nullptr && m_numBytes > 0,
			"Invalid construction arguments received");
	}


	HeapAllocation::~HeapAllocation()
	{
		SEAssert(m_numBytes > 0 ||
			(m_owningHeapPage == nullptr && m_heap == nullptr && m_baseOffset == 0),
			"Page block should be completely populated or zeroed out to signify validity/invalidity");

		Free();
	}


	HeapAllocation::HeapAllocation(HeapAllocation&& rhs) noexcept
	{
		*this = std::move(rhs);
	}


	HeapAllocation& HeapAllocation::operator=(HeapAllocation&& rhs) noexcept
	{
		if (&rhs != this)
		{
			memcpy(this, &rhs, sizeof(HeapAllocation));
			memset(&rhs, 0, sizeof(HeapAllocation));
		}
		return *this;
	}


	void HeapAllocation::Free()
	{
		if (IsValid())
		{
			m_owningHeapPage->Release(*this);
			memset(this, 0, sizeof(HeapAllocation));
		}
	}


	// -----------------------------------------------------------------------------------------------------------------


	HeapPage::HeapPage(HeapDesc const& heapDesc, uint32_t pageSize)
		: m_pageSize(pageSize)
		, m_minAlignmentSize(heapDesc.m_allowMSAATextures ? (64 * 1024) : (4 * 1024))
		, m_heapAlignment(heapDesc.m_alignment)
		, m_heap(nullptr)
		, m_threadProtector(false)
	{
		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

		// Create our heap:
		const D3D12_HEAP_DESC pageHeapDesc{
			.SizeInBytes = m_pageSize,
			.Properties = D3D12_HEAP_PROPERTIES{
				.Type = heapDesc.m_heapType,
				.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
				.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
				.CreationNodeMask = heapDesc.m_creationNodeMask,
				.VisibleNodeMask = heapDesc.m_visibleNodeMask,
			},
			.Alignment = m_heapAlignment,
			.Flags = heapDesc.m_heapFlags,
		};
		const HRESULT hr = device->CreateHeap(&pageHeapDesc, IID_PPV_ARGS(&m_heap));
		CheckHResult(hr, "Failed to create D3D12 heap for dx12::HeapPage");

		// Add the intial page allocation block metadata:
		m_freeBlocks.push_back(PageBlock(0, m_pageSize));
		m_freeBlocksBySize.emplace(m_freeBlocks.begin());
	}


	HeapPage::~HeapPage()
	{
#if defined DEBUG_MAP_RESOURCE_NAMES
		if (m_freeBlocks.size() != 1 || m_freeBlocksBySize.size() != 1 ||
			m_freeBlocks.begin()->m_baseOffset != 0 || m_freeBlocks.begin()->m_numBytes != m_pageSize)
		{
			LOG_ERROR("Not all HeapPage blocks have been released:");
			for (auto const& debugName : s_registeredResourceNames)
			{
				LOG_ERROR(util::FromWideString(debugName).c_str());
			}
		}
#endif

		{
			std::unique_lock lock(m_freeBlocksMutex);

			SEAssert(m_freeBlocks.size() == 1 && m_freeBlocksBySize.size() == 1,
				"Not all PageBlocks have been released");

			SEAssert(m_freeBlocks.begin()->m_baseOffset == 0 && 
				m_freeBlocks.begin()->m_numBytes == m_pageSize,
				"Free blocks have not been released correctly");
		}
	}


	HeapAllocation HeapPage::Allocate(uint32_t alignment, uint32_t numBytes)
	{
		SEAssert(alignment >= m_minAlignmentSize &&
			alignment <= m_heapAlignment &&
			glm::fmod(glm::log2(static_cast<float>(alignment)), 1.f) == 0 &&
			numBytes > 0,
			"Invalid allocation request");

		SEAssert(numBytes % m_minAlignmentSize == 0,
			"The requested numBytes should have already been rounded up");

		// It's valid for a request to be larger than the page size: A new page will be created to accomodate
		if (numBytes > m_pageSize)
		{
			return HeapAllocation();
		}

		{
			std::unique_lock<std::mutex> lock(m_freeBlocksMutex);

			// If we have no free blocks, or our largest free block can't fit the requested allocation, return a
			// sentinel invalid PageBlock
			if (m_freeBlocksBySize.empty() || 
				!std::prev(m_freeBlocksBySize.end())->m_pageBlockItr->CanFit(alignment, numBytes))
			{
				return HeapAllocation();
			}
			
			auto sizeItr = std::lower_bound( // 1st element NOT ordered before the PageBlock (i.e. >= )
				m_freeBlocksBySize.begin(),
				m_freeBlocksBySize.end(),
				PageBlock(0, numBytes));

			// Walk until we find a free block that can fit the number of bytes after alignment. In practice it's
			// typically only one step
			auto itr(sizeItr->m_pageBlockItr);
			while (itr != m_freeBlocks.end() && !itr->CanFit(alignment, numBytes))
			{
				++itr;
			}
			SEAssert(itr != m_freeBlocks.end() && itr->CanFit(alignment, numBytes),
				"Failed to find a valid block. This should not be possible");

			// Split the block if necessary:
			const uint32_t alignedBaseOffset = util::RoundUpToNearestMultiple(itr->m_baseOffset, alignment);
			const uint32_t endByte = (alignedBaseOffset + numBytes); // First byte off the end/out of bounds

			const uint32_t numLeadingBytes = alignedBaseOffset - itr->m_baseOffset;
			const uint32_t numTrailingBytes = (itr->m_baseOffset + itr->m_numBytes) - endByte;

			const bool remainingLeadingBytes = numLeadingBytes > 0;
			const bool remainingTrailingBytes = numTrailingBytes > 0;

			if (remainingLeadingBytes && !remainingTrailingBytes)
			{
				m_freeBlocksBySize.erase(SizeOrderedPageBlockListItr{itr});
				itr->m_numBytes = numLeadingBytes;
				m_freeBlocksBySize.emplace(itr);

				SEAssert(itr->m_numBytes > 0, "Invalid number of bytes");
			}
			else if (!remainingLeadingBytes && remainingTrailingBytes)
			{
				SEAssert(itr->m_baseOffset < endByte, "Invalid end byte");

				m_freeBlocksBySize.erase(SizeOrderedPageBlockListItr{ itr });
				itr->m_baseOffset = endByte;
				itr->m_numBytes = numTrailingBytes;
				m_freeBlocksBySize.emplace(itr);
			}
			else if (remainingLeadingBytes && remainingTrailingBytes)
			{
				// Add a new PageBlock to contain the remaining trailing bytes:
				m_freeBlocksBySize.emplace(
					m_freeBlocks.insert(
						std::next(itr),
						PageBlock(endByte, numTrailingBytes)));

				// Shrink the current PageBlock to contain the remaining leading bytes:
				m_freeBlocksBySize.erase(SizeOrderedPageBlockListItr{ itr });
				itr->m_numBytes = numLeadingBytes;
				m_freeBlocksBySize.emplace(itr);
			}
			else // Nothing to trim!
			{
				m_freeBlocksBySize.erase(SizeOrderedPageBlockListItr{ itr });
				m_freeBlocks.erase(itr);
			}

			SEAssert(endByte <= m_pageSize, "Allocation is out of bounds");

			Validate(); // _DEBUG only

			return HeapAllocation(this, m_heap.Get(), alignedBaseOffset, numBytes);
		}
	}


	bool HeapPage::IsEmpty() const
	{
		SEAssert(m_freeBlocks.size() == m_freeBlocksBySize.size(), "Page tracking is out of sync");

		return m_freeBlocks.size() == 1 && m_freeBlocks.begin()->m_numBytes == m_pageSize;
	}


	void HeapPage::Release(HeapAllocation const& resourceAllocation)
	{
		SEAssert(resourceAllocation.IsValid(), "Trying to release an invalid ResourceAllocation");

		const PageBlock pageBlock(resourceAllocation);

		// Note: m_freeBlocksMutex is already locked when we're calling this
		util::ScopedThreadProtector threadProtector(m_threadProtector);

		if (m_freeBlocks.empty())
		{
			m_freeBlocksBySize.emplace(
				m_freeBlocks.insert(m_freeBlocks.begin(), pageBlock));
		}
		else if (m_freeBlocks.size() == 1)
		{
			const uint32_t pageBlockEndByte = pageBlock.m_baseOffset + pageBlock.m_numBytes; // 1st byte off the end/OOB

			if (pageBlockEndByte == m_freeBlocks.front().m_baseOffset) // Insert to head by merging
			{
				m_freeBlocksBySize.erase(SizeOrderedPageBlockListItr{ m_freeBlocks.begin() });
				m_freeBlocks.begin()->m_baseOffset = pageBlock.m_baseOffset;
				m_freeBlocks.begin()->m_numBytes += pageBlock.m_numBytes;
				m_freeBlocksBySize.emplace(m_freeBlocks.begin());
			}
			else if (pageBlockEndByte < m_freeBlocks.front().m_baseOffset) // Insert to head
			{
				m_freeBlocksBySize.emplace(
					m_freeBlocks.insert(m_freeBlocks.begin(), pageBlock));
			}
			else // Insert to tail:
			{
				// We use m_freeBlocks.begin() here, as m_freeBlocks.size() == 1:
				const uint32_t currentBlockEndByte = 
					m_freeBlocks.begin()->m_baseOffset + m_freeBlocks.begin()->m_numBytes;

				if (currentBlockEndByte == pageBlock.m_baseOffset) // Insert to tail by merging
				{
					m_freeBlocksBySize.erase(SizeOrderedPageBlockListItr{ m_freeBlocks.begin() });
					m_freeBlocks.begin()->m_numBytes += pageBlock.m_numBytes;
					m_freeBlocksBySize.emplace(m_freeBlocks.begin());
				}
				else // Insert to tail as a new block:
				{
					SEAssert(currentBlockEndByte < pageBlock.m_baseOffset,
						"Current block extends past the allocation base offset. This should not be possible");

					m_freeBlocksBySize.emplace(
						m_freeBlocks.insert(m_freeBlocks.end(), pageBlock));
				}
			}
		}
		else // We're inserting into a list of 2 or more existing blocks:
		{
			auto itr = std::upper_bound( // First element ordered after our heap allocation
				m_freeBlocks.begin(),
				m_freeBlocks.end(),
				pageBlock,
				[](PageBlock const& a, PageBlock const& b)
				{
					return a.m_baseOffset < b.m_baseOffset;
				});

			if (itr == m_freeBlocks.begin()) // Insert to head
			{
				const uint32_t pageBlockEndByte = pageBlock.m_baseOffset + pageBlock.m_numBytes;

				if (pageBlockEndByte == itr->m_baseOffset) // Insert to head by merging
				{
					m_freeBlocksBySize.erase(SizeOrderedPageBlockListItr{ itr });
					itr->m_baseOffset -= pageBlock.m_numBytes;
					itr->m_numBytes += pageBlock.m_numBytes;
					m_freeBlocksBySize.emplace(itr);
				}
				else // Insert to head 
				{
					SEAssert(pageBlockEndByte < itr->m_baseOffset,
						"Resource allocation extends past the iterator base offset. This should not be possible");

					m_freeBlocksBySize.emplace(
						m_freeBlocks.insert(itr, pageBlock));
				}
			}
			else if (itr == m_freeBlocks.end()) // Insert to tail
			{
				auto prevItr = std::prev(itr);

				const uint32_t prevBlockEndByte = prevItr->m_baseOffset + prevItr->m_numBytes;

				if (prevBlockEndByte == pageBlock.m_baseOffset) // Insert to tail by merging
				{
					m_freeBlocksBySize.erase(SizeOrderedPageBlockListItr{ prevItr });
					prevItr->m_numBytes += pageBlock.m_numBytes;
					m_freeBlocksBySize.emplace(prevItr);
				}
				else // Insert to tail as a new block:
				{
					SEAssert(prevBlockEndByte < pageBlock.m_baseOffset,
						"Previous block extends past the resource allocation base offset. This should not be possible");

					m_freeBlocksBySize.emplace(
						m_freeBlocks.insert(m_freeBlocks.end(), pageBlock));
				}
			}
			else // Insert between 2 blocks:
			{
				auto prevItr = std::prev(itr);

				const uint32_t prevEndByte = prevItr->m_baseOffset + prevItr->m_numBytes;
				const uint32_t pageBlockEndByte = pageBlock.m_baseOffset + pageBlock.m_numBytes;

				const bool combineWithPrev = (prevEndByte == pageBlock.m_baseOffset);
				const bool combineWithItr = (pageBlockEndByte == itr->m_baseOffset);

				if (combineWithPrev && !combineWithItr)
				{
					m_freeBlocksBySize.erase(SizeOrderedPageBlockListItr{ prevItr });
					prevItr->m_numBytes += pageBlock.m_numBytes;
					m_freeBlocksBySize.emplace(prevItr);
				}
				else if (!combineWithPrev && combineWithItr)
				{
					m_freeBlocksBySize.erase(SizeOrderedPageBlockListItr{ itr });
					itr->m_baseOffset -= pageBlock.m_numBytes;
					itr->m_numBytes += pageBlock.m_numBytes;;
					m_freeBlocksBySize.emplace(itr);
				}
				else if (combineWithPrev && combineWithItr)
				{
					m_freeBlocksBySize.erase(SizeOrderedPageBlockListItr{ prevItr });
					prevItr->m_numBytes += pageBlock.m_numBytes + itr->m_numBytes;
					m_freeBlocksBySize.emplace(prevItr);

					m_freeBlocksBySize.erase(SizeOrderedPageBlockListItr{ itr });
					m_freeBlocks.erase(itr);					
				}
				else
				{
					m_freeBlocksBySize.emplace(
						m_freeBlocks.insert(itr, pageBlock));
				}
			}
		}

		Validate(); // _DEBUG only
	}


	void HeapPage::Validate()
	{
		// NOTE: Mutexes are not locked here (which is bad) but this is only for debug

#if defined(_DEBUG) && defined(ENABLE_RESOURCE_PAGE_VALIDATION)

		SEAssert(m_freeBlocks.size() == m_freeBlocksBySize.size(), "Free block maps are out of sync");

		auto itr = m_freeBlocks.begin();
		while(itr != m_freeBlocks.end())
		{
			SEAssert(m_freeBlocksBySize.contains(SizeOrderedPageBlockListItr{ itr }),
				"Block not found in the size-ordered map");

			auto next = std::next(itr);
			if (next != m_freeBlocks.end())
			{
				SEAssert(itr->m_baseOffset < m_pageSize && itr->m_numBytes <= m_pageSize,
					"Found an out of bounds value, this suggests an under/overflow");
				SEAssert(itr->m_baseOffset + itr->m_numBytes < next->m_baseOffset,
					"Found overlapping blocks");
			}
			++itr;
		}
#endif
	}


	// -----------------------------------------------------------------------------------------------------------------


	HeapDesc::HeapDesc(
		D3D12_HEAP_TYPE heapType, 
		uint32_t alignment, 
		bool allowMSAATextures, 
		uint32_t creationNodeMask, 
		uint32_t visibleNodeMask)
		: m_heapType(heapType)
		, m_heapFlags(D3D12_HEAP_FLAG_CREATE_NOT_ZEROED)
		, m_alignment(alignment)
		, m_creationNodeMask(creationNodeMask)
		, m_visibleNodeMask(visibleNodeMask)
		, m_allowMSAATextures(allowMSAATextures)
	{
	}


	PagedResourceHeap::PagedResourceHeap(HeapDesc const& heapDesc)
		: m_heapDesc(heapDesc)
		, m_alignment(m_heapDesc.m_alignment)
		, m_threadProtector(false)
	{
		ValidateHeapConfig(m_heapDesc, m_alignment); // _DEBUG only
	}


	HeapAllocation PagedResourceHeap::GetAllocation(uint32_t numBytes)
	{
		{
			util::ScopedThreadProtector threadProtector(m_threadProtector);

			for (auto& resourcePage : m_pages)
			{
				HeapAllocation requestedAllocation = resourcePage->Allocate(m_alignment, numBytes);
				if (requestedAllocation.IsValid())
				{
					return requestedAllocation;
				}
			}
		}

		// If we made it this far, no page can fit the allocation (or there are no pages yet)
		{
			util::ScopedThreadProtector threadProtector(m_threadProtector);

			// We support dynamic page sizes: Try to use the default page size, unless a larger request is made
			const uint32_t pageSize = std::max(
				k_defaultPageSize,
				numBytes = util::RoundUpToNearestMultiple(numBytes, m_alignment) );

			m_pages.emplace_back(std::make_unique<HeapPage>(m_heapDesc, pageSize));
			
			HeapAllocation requestedAllocation = m_pages.back()->Allocate(m_alignment, numBytes);
			SEAssert(requestedAllocation.IsValid(),
				"Allocation request was made on a brand new page. Failure should not be possible");
			
			return requestedAllocation;
		}
	}


	void PagedResourceHeap::EndOfFrame()
	{
		{
			util::ScopedThreadProtector threadProtector(m_threadProtector);

			// Free any pages that have been empty for k_numEmptyFramesBeforePageRelease frames
			std::unordered_map<HeapPage const*, uint8_t> emptyPageFrameCount;

			size_t pageIdx = 0;
			for (size_t i = 0; i < m_pages.size(); ++i)
			{
				if (m_pages[pageIdx]->IsEmpty())
				{
					HeapPage const* pagePtr = m_pages[pageIdx].get();
					uint8_t emptyCount = 1;

					auto emptyPageItr = m_emptyPageFrameCount.find(pagePtr);
					if (emptyPageItr != m_emptyPageFrameCount.end())
					{
						emptyCount += emptyPageItr->second;
					}

					if (emptyCount >= k_numEmptyFramesBeforePageRelease)
					{
						std::iter_swap(m_pages.begin() + pageIdx, m_pages.end() - 1);
						m_pages.pop_back();
					}
					else
					{
						emptyPageFrameCount.emplace(pagePtr, emptyCount);
						pageIdx++; // Only increment if we didn't swap'n'pop
					}
				}
				else
				{
					pageIdx++; // Only increment if we didn't swap'n'pop
				}
			}

			m_emptyPageFrameCount = std::move(emptyPageFrameCount);
		}
	}


	// -----------------------------------------------------------------------------------------------------------------


	GPUResource::GPUResource(
		Microsoft::WRL::ComPtr<ID3D12Resource> existingResource, D3D12_RESOURCE_STATES initialState, wchar_t const* name)
		: m_resource(existingResource)
		, m_heapManager(nullptr)
	{
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		m_heapManager = &context->GetHeapManager();

		SetName(name);

		D3D12_RESOURCE_DESC const& existingResourceDesc = existingResource->GetDesc();

		context->GetGlobalResourceStates().RegisterResource(
			m_resource.Get(),
			initialState,
			GetNumberOfSubresources(existingResourceDesc));
	}


	GPUResource::GPUResource(HeapManager* heapMgr, ResourceDesc const& committedResourceDesc, wchar_t const* name, PrivateCTORToken)
		: m_resource(nullptr)
		, m_heapManager(heapMgr)
	{
		// Create a committed GPU resource:
		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

		const CD3DX12_HEAP_PROPERTIES heapProperties(committedResourceDesc.m_heapType);

		D3D12_CLEAR_VALUE const* clearVal = nullptr;
		if (committedResourceDesc.m_resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER &&
			((committedResourceDesc.m_resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) ||
				(committedResourceDesc.m_resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)))
		{
			clearVal = &committedResourceDesc.m_optimizedClearValue;
		}

		const HRESULT hr = device->CreateCommittedResource(
			&heapProperties,					// Heap properties
			D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,	// Flags
			&committedResourceDesc.m_resourceDesc,		// Resource desc
			committedResourceDesc.m_initialState,
			clearVal,							// Optimized clear value
			IID_PPV_ARGS(&m_resource));
		CheckHResult(hr, "Failed to create committed resource for mutable buffer");

		SetName(name);

		// Register the resource with the state tracker:
		re::Context::GetAs<dx12::Context*>()->GetGlobalResourceStates().RegisterResource(
			m_resource.Get(),
			committedResourceDesc.m_initialState,
			GetNumberOfSubresources(committedResourceDesc.m_resourceDesc));
	}


	GPUResource::GPUResource(
		HeapManager* owningHeapMgr,
		ResourceDesc const& resourceDesc,
		HeapAllocation&& heapAllocation,
		wchar_t const* name,
		PrivateCTORToken)
		: m_heapAllocation(std::move(heapAllocation))
		, m_resource(nullptr)
		, m_heapManager(owningHeapMgr)
	{
		SEAssert(m_heapAllocation.IsValid(), "Cannot construct a resource with an invalid heap allocation");

		D3D12_CLEAR_VALUE const* clearVal = nullptr;
		if (resourceDesc.m_resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER &&
			((resourceDesc.m_resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) ||
				(resourceDesc.m_resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)))
		{
			clearVal = &resourceDesc.m_optimizedClearValue;
		}

		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

		const HRESULT hr = device->CreatePlacedResource(
			m_heapAllocation.GetHeap(),
			m_heapAllocation.GetBaseOffset(),
			&resourceDesc.m_resourceDesc,
			resourceDesc.m_initialState,
			clearVal,
			IID_PPV_ARGS(&m_resource));
		CheckHResult(hr, "Failed to create placed resource");

		SetName(name);

		// Register the resource with the state tracker:
		re::Context::GetAs<dx12::Context*>()->GetGlobalResourceStates().RegisterResource(
			m_resource.Get(),
			resourceDesc.m_initialState,
			GetNumberOfSubresources(resourceDesc.m_resourceDesc));
	}


	GPUResource::GPUResource(GPUResource&& rhs) noexcept
	{
		*this = std::move(rhs);
	}


	GPUResource& GPUResource::operator=(GPUResource&& rhs) noexcept
	{
		if (this != &rhs)
		{
			m_heapAllocation = std::move(rhs.m_heapAllocation);
			SEAssert(!rhs.m_heapAllocation.IsValid(), "Heap allocation should no longer be valid");

			m_resource = rhs.m_resource;
			rhs.m_resource = nullptr;

			m_heapManager = rhs.m_heapManager;
			rhs.m_heapManager = nullptr;
		}
		return *this;
	}


	GPUResource::~GPUResource()
	{
		if (IsValid())
		{
#if defined DEBUG_MAP_RESOURCE_NAMES
			{
				std::unique_lock<std::mutex> lock(s_registeredResourceNamesMutex);
				std::wstring const& debugName = dx12::GetWDebugName(m_resource.Get());
				if (s_registeredResourceNames.contains(debugName))
				{
					s_registeredResourceNames.erase(debugName);
				}
			}
#endif
			Free(); // Register for deferred deletion
		}
		else if (m_resource)
		{
			// If we're here, the resource is being destroyed from the HeapManager's deferred delete queue. Unregister
			// our resource from the state tracker before we're destroyed
			re::Context::GetAs<dx12::Context*>()->GetGlobalResourceStates().UnregisterResource(m_resource.Get());
		}
	}


	void GPUResource::SetName(wchar_t const* debugName)
	{
		m_resource->SetName(debugName);

#if defined DEBUG_MAP_RESOURCE_NAMES
		{
			std::unique_lock<std::mutex> lock(s_registeredResourceNamesMutex);
			s_registeredResourceNames.emplace(debugName);
		}
#endif
	}


	void GPUResource::Free()
	{
		m_heapManager->Release(*this);
		m_heapManager = nullptr;
	}


	HeapManager::HeapManager()
		: m_device(nullptr)
		, m_numFramesInFlight(0)
		, m_canMixResourceTypes(false)
	{
	}


	HeapManager::~HeapManager()
	{
		{
			std::scoped_lock lock(m_pagedHeapsMutex, m_deferredGPUResourceDeletionsMutex);
			SEAssert(m_pagedHeaps.empty(), "Paged heaps have not been cleared");
			SEAssert(m_deferredGPUResourceDeletions.empty(), "Deferred deletions queue has not been cleared");
		}
	}


	void HeapManager::Destroy()
	{
		EndOfFrame(std::numeric_limits<uint64_t>::max());

		{
			std::scoped_lock lock(m_pagedHeapsMutex, m_deferredGPUResourceDeletionsMutex);
			m_pagedHeaps.clear();
			m_deferredGPUResourceDeletions = std::queue<std::pair<uint64_t, GPUResource>>();
		}
	}

	
	void HeapManager::Initialize()
	{
		m_device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();
		m_numFramesInFlight = re::RenderManager::Get()->GetNumFramesInFlight();

		const D3D12_RESOURCE_HEAP_TIER heapTier = dx12::SysInfo::GetResourceHeapTier();
		switch (heapTier)
		{
		case D3D12_RESOURCE_HEAP_TIER_1:
		{
			m_canMixResourceTypes = false;
		}
		break;
		case D3D12_RESOURCE_HEAP_TIER_2:
		{
			m_canMixResourceTypes = true;
		}
		break;
		default: SEAssertF("Invalid heap tier");
		}
	}


	void HeapManager::EndOfFrame(uint64_t frameNum)
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_deferredGPUResourceDeletionsMutex);

			// We must clear the deferred delete queue at the end of the frame once our command lists are closed
			while (!m_deferredGPUResourceDeletions.empty() &&
				m_deferredGPUResourceDeletions.front().first + m_numFramesInFlight < frameNum)
			{
				m_deferredGPUResourceDeletions.pop();
			}
		}

		{
			std::unique_lock<std::shared_mutex> lock(m_pagedHeapsMutex);

			for (auto& pagedHeap : m_pagedHeaps)
			{
				pagedHeap.second->EndOfFrame();
			}
		}
	}


	std::unique_ptr<GPUResource> HeapManager::CreateResource(ResourceDesc const& resourceDesc, wchar_t const* name)
	{
		ValidateResourceDesc(resourceDesc); // _DEBUG only

		// Committed resources are simply wrapped in a GPUResource:
		if (resourceDesc.m_createAsComitted)
		{
			return std::make_unique<GPUResource>(this, resourceDesc, name, GPUResource::PrivateCTORToken{});
		}

		// We only currently support a single GPU; Just stubbing these in for readability
		const uint32_t creationNodeMask = dx12::SysInfo::GetDeviceNodeMask();
		const uint32_t visibleNodeMask = creationNodeMask; // Must be the creationNodeMask | optional extra bits

		uint32_t resourceNumBytes, resourceAlignment;
		GetResourceSizeAndAlignment(m_device, resourceDesc, resourceNumBytes, resourceAlignment);

		SEAssert(resourceAlignment > 0 &&
			glm::fmod(glm::log2(static_cast<float>(resourceAlignment)), 1.f) == 0,
			"Alignment must be a power of 2");
		
		SEAssert(resourceDesc.m_resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER ||
			resourceAlignment == D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
			"Buffers must have a 64KB alignment");

		const uint32_t destinationHeapAlignment = ResourceDescToHeapAlignment(resourceDesc);

		const DataHash resourceHeapKey =
			ComputePagedResourceHeapHash(resourceDesc, destinationHeapAlignment, m_canMixResourceTypes);

		PagedResourceHeap* pagedResourceHeap = nullptr;
		{
			std::shared_lock<std::shared_mutex> readLock(m_pagedHeapsMutex);

			if (!m_pagedHeaps.contains(resourceHeapKey))
			{
				// Switch our read lock to a write lock:
				readLock.unlock();
				std::unique_lock<std::shared_mutex>(m_pagedHeapsMutex);

				if (!m_pagedHeaps.contains(resourceHeapKey))
				{
					pagedResourceHeap = m_pagedHeaps.emplace(
						resourceHeapKey, 
						std::make_unique<PagedResourceHeap>(
							HeapDesc(
								resourceDesc.m_heapType,
								destinationHeapAlignment,
								resourceDesc.m_isMSAATexture,
								creationNodeMask,
								visibleNodeMask))).first->second.get();
				}
				else
				{
					pagedResourceHeap = m_pagedHeaps.at(resourceHeapKey).get();
				}
			}
			else
			{
				pagedResourceHeap = m_pagedHeaps.at(resourceHeapKey).get();
			}
		}

		// Now that we know which PagedResourceHeap will back our resource, we can create it
		return std::make_unique<GPUResource>(
			this,
			resourceDesc,
			pagedResourceHeap->GetAllocation(util::CheckedCast<uint32_t>(resourceNumBytes)),
			name,
			GPUResource::PrivateCTORToken{});
	}


	void HeapManager::Release(GPUResource& gpuResource)
	{
		SEAssert(gpuResource.IsValid(), "Trying to release an invalid gpuResource");
		
		{
			std::unique_lock<std::recursive_mutex> lock(m_deferredGPUResourceDeletionsMutex);

			gpuResource.Invalidate(); // Prevent recursive re-enqueing

			m_deferredGPUResourceDeletions.emplace(
				re::RenderManager::Get()->GetCurrentRenderFrameNum(),
				std::move(gpuResource));
		}
	}
}