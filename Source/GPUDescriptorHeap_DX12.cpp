// © 2022 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "DebugConfiguration.h"
#include "Debug_DX12.h"
#include "GPUDescriptorHeap_DX12.h"
#include "RenderManager.h"


namespace dx12
{
	GPUDescriptorHeap::GPUDescriptorHeap(
		ID3D12GraphicsCommandList* owningCommandList, 
		D3D12_COMMAND_LIST_TYPE owningCmdListType, 
		D3D12_DESCRIPTOR_HEAP_TYPE heapType)
		: m_owningCommandList(owningCommandList)
		, m_owningCommandListType(owningCmdListType)
		, m_heapType(heapType)
		, m_elementSize(re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>()
			->m_device.GetD3DDisplayDevice()->GetDescriptorHandleIncrementSize(heapType))
		, m_descriptorHeap(nullptr)
		, m_descriptorHandleCache{0}
		, m_descriptorTables{0}
		, m_descriptorTableIdxBitmask(0)
		, m_dirtyDescriptorTableIdxBitmask(0)
	{
		SEAssert("Invalid command list", m_owningCommandList != nullptr);

		SEAssert("Unexpected owning command list type", 
			owningCmdListType == D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT ||
			owningCmdListType == D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE);

		SEAssert("Descriptor heap must have a type that is not bound directly to a command list", 
			heapType == D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
			heapType == D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		// Zero-initialize:
		for (D3D12_CPU_DESCRIPTOR_HANDLE& descriptorHandle : m_descriptorHandleCache)
		{
			descriptorHandle = { 0 };
		}
		for (DescriptorTableCache& rootSigDescriptorTable : m_descriptorTables)
		{
			rootSigDescriptorTable = { 0, 0 };
		}
		for (uint32_t inlineDescriptorType = 0; inlineDescriptorType < InlineRootType_Count; inlineDescriptorType++)
		{
			for (D3D12_GPU_VIRTUAL_ADDRESS& gpuVirtualAddr : m_inlineDescriptors[inlineDescriptorType])
			{
				gpuVirtualAddr = 0;
			}
		}
		for (uint32_t inlineBitmaskIdx = 0; inlineBitmaskIdx < InlineRootType_Count; inlineBitmaskIdx++)
		{
			m_dirtyInlineDescriptorIdxBitmask[inlineBitmaskIdx] = 0;
		}


		// Create our GPU-visible descriptor heap:
		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

		D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
		descriptorHeapDesc.Type = m_heapType;
		descriptorHeapDesc.NumDescriptors = k_totalDescriptors;
		descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		descriptorHeapDesc.NodeMask = deviceNodeMask;

		HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_descriptorHeap));
		CheckHResult(hr, "Failed to create descriptor heap");
	}


	GPUDescriptorHeap::~GPUDescriptorHeap()
	{
		m_descriptorHeap = nullptr;
	}


	ID3D12DescriptorHeap* GPUDescriptorHeap::GetD3DDescriptorHeap() const
	{
		return m_descriptorHeap.Get();
	}


	void GPUDescriptorHeap::ParseRootSignatureDescriptorTables(dx12::RootSignature const& rootSig)
	{
		D3D12_ROOT_SIGNATURE_DESC1 const& rootSigDesc = rootSig.GetD3DRootSignatureDesc();
		
		m_descriptorTableIdxBitmask = rootSig.GetDescriptorTableIdxBitmask();

		uint32_t offset = 0;
		uint32_t rootIdxBit = 0; // Updated immediately...
		for (uint32_t rootIdx = 0; rootIdx < k_totalRootSigDescriptorTableIndices && rootIdx < rootSigDesc.NumParameters; rootIdx++)
		{
			rootIdxBit = (1 << rootIdx); // 1, 10, 100, 1000, ..., 1000 0000 0000 0000 0000 0000 0000 0000

			if (m_descriptorTableIdxBitmask & rootIdxBit)
			{
				const uint32_t numDescriptors = rootSig.GetNumDescriptors(rootIdx);

				// Update our cache:
				m_descriptorTables[rootIdx].m_baseDescriptor = &m_descriptorHandleCache[offset];
				m_descriptorTables[rootIdx].m_numElements = numDescriptors;

				offset += numDescriptors;
			}
		}
		SEAssert("Offset is out of bounds. There are not enough descriptors allocated", offset < k_totalDescriptors);

		// Remove all dirty flags: We'll need to call Set___() in order to mark any descriptors for copying
		m_dirtyDescriptorTableIdxBitmask = 0;
	}


	void GPUDescriptorHeap::SetDescriptorTable(
		uint32_t rootParamIdx, const D3D12_CPU_DESCRIPTOR_HANDLE src, uint32_t offset, uint32_t count)
	{
		SEAssert("Invalid root parameter index", rootParamIdx < k_totalRootSigDescriptorTableIndices);
		SEAssert("Source cannot be null", src.ptr != 0);
		SEAssert("Invalid offset", offset < k_totalDescriptors);
		SEAssert("Too many descriptors", count < k_totalDescriptors);

		DescriptorTableCache& destDescriptorTable = m_descriptorTables[rootParamIdx];

		SEAssert("Writing too many descriptors from the given offset", 
			offset + count <= destDescriptorTable.m_numElements);

		// Make a local copy of the source descriptor data:
		D3D12_CPU_DESCRIPTOR_HANDLE* destDescriptorHandle = destDescriptorTable.m_baseDescriptor + offset;
		for (uint32_t currentDescriptor = 0; currentDescriptor < count; currentDescriptor++)
		{
			const size_t baseOffset = currentDescriptor * m_elementSize;
			destDescriptorHandle[currentDescriptor] = D3D12_CPU_DESCRIPTOR_HANDLE(src.ptr + baseOffset);
		}

		// Mark our root parameter index as dirty:
		m_dirtyDescriptorTableIdxBitmask |= (1 << rootParamIdx);
	}


	void GPUDescriptorHeap::SetInlineCBV(uint32_t rootParamIdx, ID3D12Resource* buffer)
	{
		SEAssert("Invalid root parameter index", rootParamIdx < k_totalRootSigDescriptorTableIndices);
		SEAssert("Invalid resource pointer", buffer != nullptr);

		m_inlineDescriptors[CBV][rootParamIdx] = buffer->GetGPUVirtualAddress();
		
		// Mark our root parameter index as dirty:
		m_dirtyInlineDescriptorIdxBitmask[CBV] |= (1 << rootParamIdx);
	}


	void GPUDescriptorHeap::SetInlineSRV(uint32_t rootParamIdx, ID3D12Resource* buffer)
	{
		SEAssert("Invalid root parameter index", rootParamIdx < k_totalRootSigDescriptorTableIndices);
		SEAssert("Invalid resource pointer", buffer != nullptr);

		m_inlineDescriptors[SRV][rootParamIdx] = buffer->GetGPUVirtualAddress();

		// Mark our root parameter index as dirty:
		m_dirtyInlineDescriptorIdxBitmask[SRV] |= (1 << rootParamIdx);
	}


	void GPUDescriptorHeap::SetInlineUAV(uint32_t rootParamIdx, ID3D12Resource* buffer)
	{
		SEAssert("Invalid root parameter index", rootParamIdx < k_totalRootSigDescriptorTableIndices);
		SEAssert("Invalid resource pointer", buffer != nullptr);

		m_inlineDescriptors[UAV][rootParamIdx] = buffer->GetGPUVirtualAddress();

		// Mark our root parameter index as dirty:
		m_dirtyInlineDescriptorIdxBitmask[UAV] |= (1 << rootParamIdx);
	}


	uint32_t GPUDescriptorHeap::GetNumDirtyTableDescriptors() const
	{
		uint32_t count = 0;

#if defined(_DEBUG)
		uint32_t dirtyDescriptorBitmask = m_dirtyDescriptorTableIdxBitmask;
#endif
		for (uint32_t i = 0; i < k_totalRootSigDescriptorTableIndices; i++)
		{
			const uint32_t bitmask = (1 << i);
			if (m_dirtyDescriptorTableIdxBitmask & bitmask)
			{
				count += m_descriptorTables[i].m_numElements;

#if defined(_DEBUG)
				dirtyDescriptorBitmask ^= bitmask;
#endif
			}
		}

#if defined(_DEBUG)
		SEAssert("Expected the bitmask to be all 0's", dirtyDescriptorBitmask == 0);
#endif

		return count;
	}


	void GPUDescriptorHeap::CommitDescriptorTables()
	{
		// Note: The commandList should have already called SetDescriptorHeaps for m_descriptorHeap

		const uint32_t numDirtyTableDescriptors = GetNumDirtyTableDescriptors();
		if (numDirtyTableDescriptors > 0)
		{
			SEAssert("Invalid descriptor heap", m_descriptorHeap != nullptr);

			dx12::Context::PlatformParams* ctxPlatParams =
				re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

			ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

			uint32_t rootIdxBit = 0; // Updated immediately...
			for (uint32_t rootIdx = 0; rootIdx < k_totalRootSigDescriptorTableIndices; rootIdx++)
			{
				rootIdxBit = (1 << rootIdx); // 1, 10, 100, 1000, ..., 1000 0000 0000 0000 0000 0000 0000 0000

				// Only copy if the descriptor table at the current root signature index has changed
				if (m_dirtyDescriptorTableIdxBitmask & rootIdxBit)
				{
					const D3D12_CPU_DESCRIPTOR_HANDLE* srcDescriptorHandle = m_descriptorTables[rootIdx].m_baseDescriptor;
					const uint32_t numSrcDescriptors = m_descriptorTables[rootIdx].m_numElements;

					const size_t dstOffset = rootIdx * m_elementSize;
					const D3D12_CPU_DESCRIPTOR_HANDLE dstDescriptorHandle = 
						{ m_descriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + dstOffset };
					
					SEAssert("Out of bounds destination", 
						dstDescriptorHandle.ptr < 
						m_descriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + (k_totalDescriptors * m_elementSize));

					device->CopyDescriptors(
						1,						// UINT NumDestDescriptorRanges
						&dstDescriptorHandle,	// const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptorRangeStarts
						&numSrcDescriptors,		// const UINT* pDestDescriptorRangeSizes
						numSrcDescriptors,		// UINT NumSrcDescriptorRanges
						srcDescriptorHandle,	// const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorRangeStarts
						nullptr,				// const UINT* pSrcDescriptorRangeSizes
						m_heapType				// D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType
					);

					const D3D12_GPU_DESCRIPTOR_HANDLE dstGPUDescriptorHandle =
						{ m_descriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + dstOffset };

					switch (m_owningCommandListType)
					{
					case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT:
					{
						m_owningCommandList->SetGraphicsRootDescriptorTable(rootIdx, dstGPUDescriptorHandle);
					}
					break;
					case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE:
					{
						m_owningCommandList->SetComputeRootDescriptorTable(rootIdx, dstGPUDescriptorHandle);
					}
					break;
					default:
						SEAssertF("Invalid root signature type");
					}

					// Flip the dirty bit now that we've updated the GPU-visible descriptor table data:
					m_dirtyDescriptorTableIdxBitmask ^= rootIdxBit;
				}	
			}
		}
	}


	void GPUDescriptorHeap::Commit()
	{
#if defined(_DEBUG)
		// Assert all of our root index bitmasks are unique:
		for (uint8_t i = 0; i < InlineRootType_Count; i++)
		{
			SEAssert("Inline descriptor index and descriptor table index overlap", 
				(m_dirtyInlineDescriptorIdxBitmask[i] & m_descriptorTableIdxBitmask) == 0);

			for (uint8_t j = 0; j < InlineRootType_Count; j++)
			{
				if (i != j)
				{
					SEAssert("Inline descriptor indexes overlap", 
						(m_dirtyInlineDescriptorIdxBitmask[i] & m_dirtyInlineDescriptorIdxBitmask[j]) == 0);
				}
			}
		}
#endif		

		CommitDescriptorTables();
		CommitInlineDescriptors();
	}


	void CommitInlineDescriptorsHelper(
		ID3D12GraphicsCommandList* commandList, 
		D3D12_COMMAND_LIST_TYPE commandListType,
		GPUDescriptorHeap::InlineDescriptorType inlineType,
		uint32_t& dirtyIdxBitmask, 
		D3D12_GPU_VIRTUAL_ADDRESS* inlineDescriptors)
	{
		if (dirtyIdxBitmask == 0)
		{
			return;
		}

		uint32_t rootIdxBit = 0; // Updated immediately...
		for (uint32_t rootIdx = 0; rootIdx < GPUDescriptorHeap::k_totalRootSigDescriptorTableIndices; rootIdx++)
		{
			rootIdxBit = (1 << rootIdx); // 1, 10, 100, 1000, ..., 1000 0000 0000 0000 0000 0000 0000 0000

			// Only copy if the descriptor table at the current root signature index has changed
			if (dirtyIdxBitmask & rootIdxBit)
			{
				switch (inlineType)
				{
				case GPUDescriptorHeap::InlineDescriptorType::CBV:
				{
					switch (commandListType)
					{
					case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT:
					{
						commandList->SetGraphicsRootConstantBufferView(rootIdx, inlineDescriptors[rootIdx]);
					}
					break;
					case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE:
					{
						commandList->SetComputeRootConstantBufferView(rootIdx, inlineDescriptors[rootIdx]);
					}
					break;
					default:
						SEAssertF("Invalid root signature type");
					}
				}
				break;
				case GPUDescriptorHeap::InlineDescriptorType::SRV:
				{
					switch (commandListType)
					{
					case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT:
					{
						commandList->SetGraphicsRootShaderResourceView(rootIdx, inlineDescriptors[rootIdx]);
					}
					break;
					case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE:
					{
						commandList->SetComputeRootShaderResourceView(rootIdx, inlineDescriptors[rootIdx]);
					}
					break;
					default:
						SEAssertF("Invalid root signature type");
					}
				}
				break;
				case GPUDescriptorHeap::InlineDescriptorType::UAV:
				{
					switch (commandListType)
					{
					case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT:
					{
						commandList->SetGraphicsRootUnorderedAccessView(rootIdx, inlineDescriptors[rootIdx]);
					}
					break;
					case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE:
					{
						commandList->SetComputeRootUnorderedAccessView(rootIdx, inlineDescriptors[rootIdx]);
					}
					break;
					default:
						SEAssertF("Invalid root signature type");
					}
				}
				break;
				default:
					SEAssertF("Invalid inline type");
				}

				// Flip the dirty bit now that we've updated the GPU-visible descriptor table data:
				dirtyIdxBitmask ^= rootIdxBit;
			}
		}
	}

	
	void GPUDescriptorHeap::CommitInlineDescriptors()
	{
		for (uint8_t inlineRootType = 0; inlineRootType < static_cast<uint8_t>(InlineRootType_Count); inlineRootType++)
		{
			CommitInlineDescriptorsHelper(
				m_owningCommandList,
				m_owningCommandListType,
				static_cast<InlineDescriptorType>(inlineRootType),
				m_dirtyInlineDescriptorIdxBitmask[inlineRootType],
				m_inlineDescriptors[inlineRootType]);
		}
	}
}