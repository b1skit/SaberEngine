// © 2022 Adam Badke. All rights reserved.
#include "Core\Assert.h"
#include "CommandList_DX12.h"
#include "Core\Config.h"
#include "Context_DX12.h"
#include "CPUDescriptorHeapManager_DX12.h"
#include "Debug_DX12.h"
#include "GPUDescriptorHeap_DX12.h"
#include "RenderManager.h"
#include "SysInfo_DX12.h"

#include <d3dx12.h>


namespace dx12
{
	GPUDescriptorHeap::GPUDescriptorHeap(
		dx12::CommandListType owningCmdListType, ID3D12GraphicsCommandList* owningCommandList)
		: m_owningCommandListType(owningCmdListType)
		, m_owningCommandList(owningCommandList)
		, m_heapType(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) // For now, this is all we ever need
		, m_elementSize(re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice()
			->GetDescriptorHandleIncrementSize(m_heapType))
		, m_gpuDescriptorTableHeap(nullptr)
		, m_gpuDescriptorTableHeapCPUBase{0}
		, m_gpuDescriptorTableHeapGPUBase{0}
		, m_cpuDescriptorTableHeapCache{0}
		, m_cpuDescriptorTableCacheLocations{0}
		, m_rootSigDescriptorTableIdxBitmask(0)
		, m_dirtyDescriptorTableIdxBitmask(0)
		, m_unsetInlineDescriptors(0)
	{
		SEAssert(m_owningCommandList != nullptr, "Invalid command list");

		SEAssert(owningCmdListType == D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT ||
			owningCmdListType == D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE,
			"Unexpected owning command list type");

		SEAssert(m_heapType == D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
			m_heapType == D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
			"Descriptor heap must have a type that is not bound directly to a command list");
		SEAssert(m_elementSize > 0, "Invalid element size");

		// Create our GPU-visible descriptor heap:
		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

		const D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = D3D12_DESCRIPTOR_HEAP_DESC{
			.Type = m_heapType,
			.NumDescriptors = k_totalDescriptors,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			.NodeMask = dx12::SysInfo::GetDeviceNodeMask() };

		HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_gpuDescriptorTableHeap));
		CheckHResult(hr, "Failed to create descriptor heap");

		// Name our descriptor heap. We extract the command list's debug name to ensure consistency
		const std::wstring extractedName = dx12::GetWDebugName(m_owningCommandList);

		const std::wstring descriptorTableHeapName = extractedName + L"_GPUDescriptorHeap";
		m_gpuDescriptorTableHeap->SetName(descriptorTableHeapName.c_str());

		// Initialize everything:
		Reset();
	}


	GPUDescriptorHeap::~GPUDescriptorHeap()
	{
		m_gpuDescriptorTableHeap = nullptr;
	}


	void GPUDescriptorHeap::Reset()
	{
		SEAssert(m_gpuDescriptorTableHeap, "Shader-visible descriptor heap is null");

		m_gpuDescriptorTableHeapCPUBase = m_gpuDescriptorTableHeap->GetCPUDescriptorHandleForHeapStart();
		m_gpuDescriptorTableHeapGPUBase = m_gpuDescriptorTableHeap->GetGPUDescriptorHandleForHeapStart();

		memset(m_cpuDescriptorTableHeapCache, 0, k_totalDescriptors * sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));
		memset(m_cpuDescriptorTableCacheLocations, 0, k_totalRootSigDescriptorTableIndices * sizeof(CPUDescriptorTableCacheMetadata));

		m_rootSigDescriptorTableIdxBitmask = 0;
		m_dirtyDescriptorTableIdxBitmask = 0;

		for (uint8_t i = 0; i < InlineRootType_Count; i++)
		{
			memset(m_inlineDescriptors[i], 0, k_totalRootSigDescriptorTableIndices * sizeof(D3D12_GPU_VIRTUAL_ADDRESS));
		}
		memset(m_dirtyInlineDescriptorIdxBitmask, 0, InlineRootType_Count * sizeof(uint32_t));

		m_unsetInlineDescriptors = std::numeric_limits<uint32_t>::max(); // Nothing has been set

		m_currentRootSig = nullptr;
	}


	void GPUDescriptorHeap::SetNullDescriptors(dx12::RootSignature const* rootSig)
	{
		std::vector<RootSignature::RootParameter> const& rootParams = rootSig->GetRootSignatureEntries();

		// Note: Root descriptors cannot be set to null:
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptors-overview#null-descriptors
		// Instead, we mark all inline descriptors we encounter in a bitmask, and remove the bits when the
		// descriptors are set for the first time. This allows us to assert that at least *something* has been
		// set in each position when we go to commit our descriptors. No point setting dummy entries as we
		// found our root params via shader reflection: we KNOW they're going to be accessed by the GPU
		// (which is guaranteed to result in undefined behavior) so something MUST be set
		m_unsetInlineDescriptors = 0;

		for (auto const& rootParam : rootParams)
		{
			const uint8_t rootIdx = rootParam.m_index;
			switch (rootParam.m_type)
			{
			case RootSignature::RootParameter::Type::DescriptorTable:
			{
				// Do nothing...
			}
			break;
			case RootSignature::RootParameter::Type::Constant:
			case RootSignature::RootParameter::Type::CBV:
			case RootSignature::RootParameter::Type::SRV:
			case RootSignature::RootParameter::Type::UAV:
			{
				m_unsetInlineDescriptors |= 1 << rootIdx;
			}
			break;
			default:
				SEAssertF("Invalid parameter type");
			}
		}

		// Parse the descriptor table metadata, and set null descriptors:
		std::vector<dx12::RootSignature::DescriptorTable> const& descriptorTableMetadata = 
			rootSig->GetDescriptorTableMetadata();

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		for (RootSignature::DescriptorTable const& descriptorTable : descriptorTableMetadata)
		{
			for (size_t rangeType = 0; rangeType < RootSignature::DescriptorType::Type_Count; rangeType++)
			{
				for (size_t rangeEntry = 0; rangeEntry < descriptorTable.m_ranges[rangeType].size(); rangeEntry++)
				{
					switch (rangeType)
					{
					case RootSignature::DescriptorType::SRV:
					{
						SetDescriptorTable(
							descriptorTable.m_index, 
							context->GetNullSRVDescriptor(
								descriptorTable.m_ranges[rangeType][rangeEntry].m_srvDesc.m_viewDimension,
								descriptorTable.m_ranges[rangeType][rangeEntry].m_srvDesc.m_format).GetBaseDescriptor(),
							static_cast<uint32_t>(rangeEntry),
							1);						
					}
					break;
					case RootSignature::DescriptorType::UAV:
					{
						SetDescriptorTable(
							descriptorTable.m_index,
							context->GetNullUAVDescriptor(
								descriptorTable.m_ranges[rangeType][rangeEntry].m_uavDesc.m_viewDimension,
								descriptorTable.m_ranges[rangeType][rangeEntry].m_uavDesc.m_format).GetBaseDescriptor(),
							static_cast<uint32_t>(rangeEntry),
						1);
					}
					break;
					case RootSignature::DescriptorType::CBV:
					{
						SEAssertF("TODO: Handle this type");
					}
					default:
						SEAssertF("Invalid range type");
					}
				}
			}			
		}
	}


	void GPUDescriptorHeap::ParseRootSignatureDescriptorTables(dx12::RootSignature const* rootSig)
	{
		m_currentRootSig = rootSig;

		const uint32_t numParams = static_cast<uint32_t>(rootSig->GetRootSignatureEntries().size());

		// Get our descriptor table bitmask: Bits map to root signature indexes containing a descriptor table
		m_rootSigDescriptorTableIdxBitmask = rootSig->GetDescriptorTableIdxBitmask();
		// TODO: Just parse the bitmask here, rather than relying on the root sig object to parse it for us

		uint32_t offset = 0;
		uint32_t rootIdxBit = 0; // Updated immediately...
		uint32_t descriptorTableIdxBitmask = m_rootSigDescriptorTableIdxBitmask;
		for (uint32_t rootIdx = 0; rootIdx < k_totalRootSigDescriptorTableIndices && rootIdx < numParams; rootIdx++)
		{
			if (descriptorTableIdxBitmask == 0)
			{
				break; // No point continuing if we've flipped the last bit back to 0
			}

			rootIdxBit = (1 << rootIdx); // 1, 10, 100, 1000, ..., 1000 0000 0000 0000 0000 0000 0000 0000

			if (descriptorTableIdxBitmask & rootIdxBit)
			{
				const uint32_t numDescriptors = rootSig->GetNumDescriptorsInTable(rootIdx);

				// Update our cache:
				m_cpuDescriptorTableCacheLocations[rootIdx].m_baseDescriptor = &m_cpuDescriptorTableHeapCache[offset];
				m_cpuDescriptorTableCacheLocations[rootIdx].m_numElements = numDescriptors;

				offset += numDescriptors;

				descriptorTableIdxBitmask ^= rootIdxBit;
			}
		}
		SEAssert(offset < k_totalDescriptors,
			"Offset is out of bounds, not enough descriptors allocated. Consider increasing k_totalDescriptors");

		// Remove all dirty flags: We'll need to call Set___() in order to mark any descriptors for copying
		m_dirtyDescriptorTableIdxBitmask = 0;

		SetNullDescriptors(rootSig);
	}


	void GPUDescriptorHeap::SetDescriptorTable(
		uint32_t rootParamIdx, D3D12_CPU_DESCRIPTOR_HANDLE src, uint32_t offset, uint32_t count)
	{
		SEAssert(rootParamIdx < k_totalRootSigDescriptorTableIndices, "Invalid root parameter index");
		SEAssert(src.ptr != 0, "Source cannot be null");
		SEAssert(offset < k_totalDescriptors, "Invalid offset");
		SEAssert(count < k_totalDescriptors, "Too many descriptors");

		// TODO: Handle this for Sampler heap type

		CPUDescriptorTableCacheMetadata const& destCPUDescriptorTable = m_cpuDescriptorTableCacheLocations[rootParamIdx];

		SEAssert(offset + count <= destCPUDescriptorTable.m_numElements,
			"Writing too many descriptors from the given offset");

		// Make a local copy of the source descriptor(s):
		D3D12_CPU_DESCRIPTOR_HANDLE* destDescriptorHandle = destCPUDescriptorTable.m_baseDescriptor + offset;
		for (uint32_t currentDescriptor = 0; currentDescriptor < count; currentDescriptor++)
		{
			const size_t srcBaseOffset = currentDescriptor * m_elementSize;
			destDescriptorHandle[currentDescriptor] = D3D12_CPU_DESCRIPTOR_HANDLE(src.ptr + srcBaseOffset);
		}

		// Mark our root parameter index as dirty:
		m_dirtyDescriptorTableIdxBitmask |= (1 << rootParamIdx);
	}


	// https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html#using-descriptors-directly-in-the-root-arguments
	void GPUDescriptorHeap::SetInlineCBV(uint32_t rootParamIdx, ID3D12Resource* buffer, uint64_t alignedByteOffset)
	{
		SEAssert(rootParamIdx < k_totalRootSigDescriptorTableIndices, "Invalid root parameter index");
		SEAssert(buffer != nullptr, "Invalid resource pointer");
		SEAssert(m_heapType == D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "Wrong heap type");

		m_inlineDescriptors[CBV][rootParamIdx] = buffer->GetGPUVirtualAddress() + alignedByteOffset;
		
		// Mark our root parameter index as dirty:
		const uint32_t rootParamIdxBitmask = 1 << rootParamIdx;

		m_dirtyInlineDescriptorIdxBitmask[CBV] |= rootParamIdxBitmask;

		if (m_unsetInlineDescriptors & rootParamIdxBitmask)
		{
			// The inline root parameter at this index has been set at least once: Remove the unset flag
			m_unsetInlineDescriptors ^= rootParamIdxBitmask;
		}
	}


	// https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html#using-descriptors-directly-in-the-root-arguments
	void GPUDescriptorHeap::SetInlineSRV(uint32_t rootParamIdx, ID3D12Resource* buffer, uint64_t alignedByteOffset)
	{
		SEAssert(rootParamIdx < k_totalRootSigDescriptorTableIndices, "Invalid root parameter index");
		SEAssert(buffer != nullptr, "Invalid resource pointer");
		SEAssert(m_heapType == D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "Wrong heap type");

		m_inlineDescriptors[SRV][rootParamIdx] = buffer->GetGPUVirtualAddress() + alignedByteOffset;

		// Mark our root parameter index as dirty:
		const uint32_t rootParamIdxBitmask = 1 << rootParamIdx;
		
		m_dirtyInlineDescriptorIdxBitmask[SRV] |= rootParamIdxBitmask;

		if (m_unsetInlineDescriptors & rootParamIdxBitmask)
		{
			// The inline root parameter at this index has been set at least once: Remove the unset flag
			m_unsetInlineDescriptors ^= rootParamIdxBitmask;
		}
	}


	// https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html#using-descriptors-directly-in-the-root-arguments
	void GPUDescriptorHeap::SetInlineUAV(uint32_t rootParamIdx, ID3D12Resource* buffer, uint64_t alignedByteOffset)
	{
		SEAssert(rootParamIdx < k_totalRootSigDescriptorTableIndices, "Invalid root parameter index");
		SEAssert(buffer != nullptr, "Invalid resource pointer");
		SEAssert(m_heapType == D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "Wrong heap type");
		
		m_inlineDescriptors[UAV][rootParamIdx] = buffer->GetGPUVirtualAddress() + alignedByteOffset;

		// Mark our root parameter index as dirty:
		const uint32_t rootParamIdxBitmask = 1 << rootParamIdx;

		m_dirtyInlineDescriptorIdxBitmask[UAV] |= rootParamIdxBitmask;
		
		if (m_unsetInlineDescriptors & rootParamIdxBitmask)
		{
			// The inline root parameter at this index has been set at least once: Remove the unset flag
			m_unsetInlineDescriptors ^= rootParamIdxBitmask;
		}
	}


	void GPUDescriptorHeap::Commit()
	{
#if defined(_DEBUG)
		// Debug: Assert all of our root index bitmasks are unique
		if (core::Config::Get()->GetValue<int>(core::configkeys::k_debugLevelCmdLineArg) > 0)
		{
			for (uint8_t i = 0; i < InlineRootType_Count; i++)
			{
				SEAssert((m_dirtyInlineDescriptorIdxBitmask[i] & m_rootSigDescriptorTableIdxBitmask) == 0,
					"Inline descriptor index and descriptor table index overlap");

				for (uint8_t j = 0; j < InlineRootType_Count; j++)
				{
					if (i != j)
					{
						SEAssert((m_dirtyInlineDescriptorIdxBitmask[i] & m_dirtyInlineDescriptorIdxBitmask[j]) == 0,
							"Inline descriptor indexes overlap");
					}
				}
			}
		}
#endif		
		CommitDescriptorTables();
		CommitInlineDescriptors();
	}


	void GPUDescriptorHeap::CommitDescriptorTables()
	{
		// Note: The commandList should have already called SetDescriptorHeaps for m_gpuDescriptorTableHeap

		const uint32_t numDirtyTableDescriptors = GetNumDirtyTableDescriptors();
		if (numDirtyTableDescriptors > 0)
		{
			SEAssert(m_gpuDescriptorTableHeap != nullptr, "Invalid descriptor heap");

			ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

			uint32_t rootIdxBit = 0; // Updated immediately...
			for (uint32_t rootIdx = 0; rootIdx < k_totalRootSigDescriptorTableIndices; rootIdx++)
			{
				if (m_dirtyDescriptorTableIdxBitmask == 0)
				{
					break; // No point continuing if we've flipped the last bit back to 0
				}

				rootIdxBit = (1 << rootIdx); // 1, 10, 100, 1000, ..., 1000 0000 0000 0000 0000 0000 0000 0000

				// Only copy if the descriptor table at the current root signature index has changed
				if (m_dirtyDescriptorTableIdxBitmask & rootIdxBit)
				{
					const D3D12_CPU_DESCRIPTOR_HANDLE* tableBaseDescriptor =
						m_cpuDescriptorTableCacheLocations[rootIdx].m_baseDescriptor;
					const uint32_t numTableDescriptors = m_cpuDescriptorTableCacheLocations[rootIdx].m_numElements;

					const size_t tableSize = numTableDescriptors * m_elementSize;

					SEAssert(m_gpuDescriptorTableHeapCPUBase.ptr + tableSize <=
							m_gpuDescriptorTableHeap->GetCPUDescriptorHandleForHeapStart().ptr + (k_totalDescriptors * m_elementSize),
						"Out of bounds CPU destination. Consider increasing k_totalDescriptors");

					SEAssert(m_gpuDescriptorTableHeapGPUBase.ptr + tableSize <=
							m_gpuDescriptorTableHeap->GetGPUDescriptorHandleForHeapStart().ptr + (k_totalDescriptors * m_elementSize),
						"Out of bounds GPU destination. Consider increasing k_totalDescriptors");

					device->CopyDescriptors(
						1,									// UINT NumDestDescriptorRanges
						&m_gpuDescriptorTableHeapCPUBase,	// const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptorRangeStarts
						&numTableDescriptors,				// const UINT* pDestDescriptorRangeSizes
						numTableDescriptors,				// UINT NumSrcDescriptorRanges
						tableBaseDescriptor,				// const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorRangeStarts
						nullptr,							// const UINT* pSrcDescriptorRangeSizes
						m_heapType							// D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType
					);

					switch (m_owningCommandListType)
					{
					case dx12::CommandListType::Direct:
					{
						m_owningCommandList->SetGraphicsRootDescriptorTable(rootIdx, m_gpuDescriptorTableHeapGPUBase);
					}
					break;
					case dx12::CommandListType::Compute:
					{
						m_owningCommandList->SetComputeRootDescriptorTable(rootIdx, m_gpuDescriptorTableHeapGPUBase);
					}
					break;
					default:
						SEAssertF("Invalid root signature type");
					}

					// Increment our stack pointers:
					m_gpuDescriptorTableHeapCPUBase.ptr += tableSize;
					m_gpuDescriptorTableHeapGPUBase.ptr += tableSize;

					// Flip the dirty bit now that we've updated the GPU-visible descriptor table data:
					m_dirtyDescriptorTableIdxBitmask ^= rootIdxBit;
				}	
			}
		}
	}


	void CommitInlineDescriptorsHelper(
		ID3D12GraphicsCommandList* commandList, 
		dx12::CommandListType commandListType,
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
			if (dirtyIdxBitmask == 0)
			{
				break; // No point continuing if we've flipped the last bit back to 0
			}

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
					case dx12::CommandListType::Direct:
					{
						commandList->SetGraphicsRootConstantBufferView(rootIdx, inlineDescriptors[rootIdx]);
					}
					break;
					case dx12::CommandListType::Compute:
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
					case dx12::CommandListType::Direct:
					{
						commandList->SetGraphicsRootShaderResourceView(rootIdx, inlineDescriptors[rootIdx]);
					}
					break;
					case dx12::CommandListType::Compute:
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
					case dx12::CommandListType::Direct:
					{
						commandList->SetGraphicsRootUnorderedAccessView(rootIdx, inlineDescriptors[rootIdx]);
					}
					break;
					case dx12::CommandListType::Compute:
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
		// Debug: Catch unset descriptors
		if (m_unsetInlineDescriptors != 0)
		{
			std::string unsetInlineDescriptorNames;

			std::vector<RootSignature::RootParameter> const& rootParams = m_currentRootSig->GetRootSignatureEntries();

			for (auto const& rootParam : rootParams)
			{
				if (m_unsetInlineDescriptors & (1 << rootParam.m_index))
				{
					unsetInlineDescriptorNames += m_currentRootSig->DebugGetNameFromRootParamIdx(rootParam.m_index) + " ";;
				}
			}
			
			SEAssertF(std::format("An inline descriptor has not been set. Shader access will result in undefined "
				"behavior: {}",
				unsetInlineDescriptorNames).c_str());
		}



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
				count += m_cpuDescriptorTableCacheLocations[i].m_numElements;

#if defined(_DEBUG)
				dirtyDescriptorBitmask ^= bitmask;
#endif
			}
		}

#if defined(_DEBUG)
		SEAssert(dirtyDescriptorBitmask == 0, "Expected the bitmask to be all 0's");
#endif

		return count;
	}
}