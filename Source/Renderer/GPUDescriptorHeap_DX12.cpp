// © 2022 Adam Badke. All rights reserved.
#include "CommandList_DX12.h"
#include "Context_DX12.h"
#include "CPUDescriptorHeapManager_DX12.h"
#include "Debug_DX12.h"
#include "GPUDescriptorHeap_DX12.h"
#include "RenderManager.h"
#include "SysInfo_DX12.h"

#include "Core/Assert.h"
#include "Core/Config.h"

#include <d3dx12.h>


namespace dx12
{
	GPUDescriptorHeap::GPUDescriptorHeap(
		uint32_t numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE heapType, std::wstring const& debugName)
		: m_deviceCache(nullptr)
		, m_numDescriptors(numDescriptors)
		, m_heapType(heapType)
		, m_elementSize(0)
		, m_gpuDescriptorHeap(nullptr)
		, m_gpuDescriptorHeapCPUBase{0}
		, m_gpuDescriptorHeapGPUBase{0}
		, m_cpuDescriptorHeapCacheLocations{0}
		, m_rootSigDescriptorTableIdxBitmask(0)
		, m_dirtyDescriptorTableIdxBitmask(0)
		, m_unsetInlineDescriptors(0)
	{
		SEAssert(m_heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "TODO: Support additional heap types");

		SEAssert(m_heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
			m_heapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
			"Descriptor heap must have a type that is not bound directly to a command list");
		
		m_deviceCache = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDevice().Get();

		m_elementSize = m_deviceCache->GetDescriptorHandleIncrementSize(m_heapType);
		SEAssert(m_elementSize > 0, "Invalid element size");

		m_cpuDescriptorHeapCache.resize(m_numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE{0});

		// Create our GPU-visible descriptor heap:
		const D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{
			.Type = m_heapType,
			.NumDescriptors = numDescriptors,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			.NodeMask = dx12::SysInfo::GetDeviceNodeMask() };

		CheckHResult(
			m_deviceCache->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_gpuDescriptorHeap)),
			"Failed to create descriptor heap");
		
		m_gpuDescriptorHeap->SetName(debugName.c_str());

		// Initialize everything:
		Reset();
	}


	void GPUDescriptorHeap::Reset()
	{
		SEAssert(m_gpuDescriptorHeap, "Shader-visible descriptor heap is null");

		m_gpuDescriptorHeapCPUBase = m_gpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		m_gpuDescriptorHeapGPUBase = m_gpuDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

		memset(m_cpuDescriptorHeapCache.data(), 0, m_numDescriptors * sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));
		memset(m_cpuDescriptorHeapCacheLocations, 0, k_totalRootSigEntries * sizeof(CPUDescriptorCacheMetadata));

		m_rootSigDescriptorTableIdxBitmask = 0;
		m_dirtyDescriptorTableIdxBitmask = 0;

		for (uint8_t i = 0; i < InlineRootType_Count; i++)
		{
			memset(m_inlineDescriptors[i], 0, k_totalRootSigEntries * sizeof(D3D12_GPU_VIRTUAL_ADDRESS));
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
			// We'll write our descriptors for each range entry consecutively:
			uint32_t baseOffset = 0;
			for (size_t rangeType = 0; rangeType < RootSignature::DescriptorType::Type_Count; rangeType++)
			{
				for (size_t rangeIdx = 0; rangeIdx < descriptorTable.m_ranges[rangeType].size(); rangeIdx++)
				{
					dx12::RootSignature::RangeEntry const& rangeEntry = descriptorTable.m_ranges[rangeType][rangeIdx];

					switch (rangeType)
					{
					case RootSignature::DescriptorType::SRV:
					{
						D3D12_CPU_DESCRIPTOR_HANDLE const& nullSRVHandle = context->GetNullSRVDescriptor(
							rangeEntry.m_srvDesc.m_viewDimension,
							rangeEntry.m_srvDesc.m_format).GetBaseDescriptor();

						for (uint32_t bindIdx = 0; bindIdx < rangeEntry.m_bindCount; ++bindIdx)
						{
							SetDescriptorTableEntry(descriptorTable.m_index, nullSRVHandle, baseOffset + bindIdx, 1);
						}
					}
					break;
					case RootSignature::DescriptorType::UAV:
					{
						D3D12_CPU_DESCRIPTOR_HANDLE const& nullUAVHandle = context->GetNullUAVDescriptor(
							rangeEntry.m_uavDesc.m_viewDimension,
							rangeEntry.m_uavDesc.m_format).GetBaseDescriptor();

						for (uint32_t bindIdx = 0; bindIdx < rangeEntry.m_bindCount; ++bindIdx)
						{
							SetDescriptorTableEntry(descriptorTable.m_index, nullUAVHandle, baseOffset + bindIdx, 1);
						}
					}
					break;
					case RootSignature::DescriptorType::CBV:
					{
						D3D12_CPU_DESCRIPTOR_HANDLE const& nullCBVHandle = 
							context->GetNullCBVDescriptor().GetBaseDescriptor();

						for (uint32_t bindIdx = 0; bindIdx < rangeEntry.m_bindCount; ++bindIdx)
						{
							SetDescriptorTableEntry(descriptorTable.m_index, nullCBVHandle, baseOffset + bindIdx, 1);
						}
					}
					break;
					default: SEAssertF("Invalid range type");
					}

					baseOffset += rangeEntry.m_bindCount;
				}
			}			
		}
	}


	void GPUDescriptorHeap::SetRootSignature(dx12::RootSignature const* rootSig)
	{
		m_currentRootSig = rootSig;

		// Parse the root signature:
		const uint32_t numParams = static_cast<uint32_t>(rootSig->GetRootSignatureEntries().size());

		// Get our descriptor table bitmask: Bits map to root signature indexes containing a descriptor table
		m_rootSigDescriptorTableIdxBitmask = rootSig->GetDescriptorTableIdxBitmask();

		uint32_t offset = 0;
		uint32_t descriptorTableIdxBitmask = m_rootSigDescriptorTableIdxBitmask;
		for (uint32_t rootIdx = 0; rootIdx < k_totalRootSigEntries && rootIdx < numParams; rootIdx++)
		{
			if (descriptorTableIdxBitmask == 0)
			{
				break; // No point continuing if we've flipped the last bit back to 0
			}

			const uint32_t rootIdxBit = (1 << rootIdx); // 1, 10, 100, 1000, ..., 1000 0000 0000 0000 0000 0000 0000 0000

			if (descriptorTableIdxBitmask & rootIdxBit)
			{
				const uint32_t numDescriptors = rootSig->GetNumDescriptorsInTable(rootIdx);

				// Update our cache:
				m_cpuDescriptorHeapCacheLocations[rootIdx].m_baseDescriptor = &m_cpuDescriptorHeapCache[offset];
				m_cpuDescriptorHeapCacheLocations[rootIdx].m_numElements = numDescriptors;

				offset += numDescriptors;

				descriptorTableIdxBitmask ^= rootIdxBit;
			}
		}
		SEAssert(offset < m_numDescriptors,
			"Offset is out of bounds, not enough descriptors allocated. Consider increasing m_numDescriptors");

		// Remove all dirty flags: We'll need to call Set___() in order to mark any descriptors for copying
		m_dirtyDescriptorTableIdxBitmask = 0;

		SetNullDescriptors(rootSig);
	}


	void GPUDescriptorHeap::SetDescriptorTableEntry(
		uint32_t rootParamIdx, D3D12_CPU_DESCRIPTOR_HANDLE src, uint32_t offset, uint32_t count)
	{
		SEAssert(rootParamIdx < k_totalRootSigEntries, "Invalid root parameter index");
		SEAssert(src.ptr != 0, "Source cannot be null");
		SEAssert(offset < m_numDescriptors, "Invalid offset");
		SEAssert(count < m_numDescriptors, "Too many descriptors");

		// TODO: Handle this for Sampler heap type

		CPUDescriptorCacheMetadata const& destCPUDescriptorTable = m_cpuDescriptorHeapCacheLocations[rootParamIdx];

		SEAssert(offset + count <= destCPUDescriptorTable.m_numElements,
			"Writing too many descriptors from the given offset");

		// Make a local copy of the source descriptor(s):
		D3D12_CPU_DESCRIPTOR_HANDLE* destDescriptorHandle = destCPUDescriptorTable.m_baseDescriptor + offset;
		for (uint32_t destIdx = 0; destIdx < count; destIdx++)
		{
			const size_t srcBaseOffset = destIdx * m_elementSize;
			destDescriptorHandle[destIdx] = D3D12_CPU_DESCRIPTOR_HANDLE(src.ptr + srcBaseOffset);
		}

		// Mark the descriptor table at the given root parameter index as dirty:
		m_dirtyDescriptorTableIdxBitmask |= (1 << rootParamIdx);
	}


	// https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html#using-descriptors-directly-in-the-root-arguments
	void GPUDescriptorHeap::SetInlineCBV(uint32_t rootParamIdx, ID3D12Resource* buffer, uint64_t alignedByteOffset)
	{
		SEAssert(rootParamIdx < k_totalRootSigEntries, "Invalid root parameter index");
		SEAssert(buffer != nullptr, "Invalid resource pointer");
		SEAssert(m_heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "Wrong heap type");

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
		SEAssert(rootParamIdx < k_totalRootSigEntries, "Invalid root parameter index");
		SEAssert(buffer != nullptr, "Invalid resource pointer");
		SEAssert(m_heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "Wrong heap type");

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
		SEAssert(rootParamIdx < k_totalRootSigEntries, "Invalid root parameter index");
		SEAssert(buffer != nullptr, "Invalid resource pointer");
		SEAssert(m_heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "Wrong heap type");
		
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


	void GPUDescriptorHeap::Commit(dx12::CommandList& cmdList)
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
		CommitDescriptorTables(cmdList);
		CommitInlineDescriptors(cmdList);
	}


	void GPUDescriptorHeap::CommitDescriptorTables(dx12::CommandList& cmdList)
	{
		// Note: The commandList should have already called SetDescriptorHeaps for m_gpuDescriptorHeap

		const uint32_t numDirtyTableDescriptors = GetNumDirtyTableDescriptors();
		if (numDirtyTableDescriptors > 0)
		{
			SEAssert(m_gpuDescriptorHeap != nullptr, "Invalid descriptor heap");

			for (uint32_t rootIdx = 0; rootIdx < k_totalRootSigEntries; rootIdx++)
			{
				if (m_dirtyDescriptorTableIdxBitmask == 0)
				{
					break; // No point continuing if we've flipped the last bit back to 0
				}

				const uint32_t rootIdxBit = (1 << rootIdx); // 1, 10, 100, 1000, ..., 1000 0000 0000 0000 0000 0000 0000 0000

				// Only copy if the descriptor table at the current root signature index has changed
				if (m_dirtyDescriptorTableIdxBitmask & rootIdxBit)
				{
					const D3D12_CPU_DESCRIPTOR_HANDLE* srcBaseDescriptor =
						m_cpuDescriptorHeapCacheLocations[rootIdx].m_baseDescriptor;
					const uint32_t numSrcDescriptors = m_cpuDescriptorHeapCacheLocations[rootIdx].m_numElements;

					const size_t tableSize = numSrcDescriptors * m_elementSize;

					SEAssert(m_gpuDescriptorHeapCPUBase.ptr + tableSize <=
							m_gpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + (m_numDescriptors * m_elementSize),
						"Out of bounds CPU destination. Consider increasing m_numDescriptors");

					SEAssert(m_gpuDescriptorHeapGPUBase.ptr + tableSize <=
							m_gpuDescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + (m_numDescriptors * m_elementSize),
						"Out of bounds GPU destination. Consider increasing m_numDescriptors");

					// Note: Our source descriptors are not contiguous, but our destination descriptors are
					m_deviceCache->CopyDescriptors(
						1,									// UINT NumDestDescriptorRanges
						&m_gpuDescriptorHeapCPUBase,	// const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptorRangeStarts
						&numSrcDescriptors,					// const UINT* pDestDescriptorRangeSizes
						numSrcDescriptors,					// UINT NumSrcDescriptorRanges
						srcBaseDescriptor,					// const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorRangeStarts
						nullptr,							// const UINT* pSrcDescriptorRangeSizes
						m_heapType							// D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType
					);

					// Record the descriptor table address in the root sig:
					ID3D12GraphicsCommandList* d3dCmdList = cmdList.GetD3DCommandList().Get();
					switch (cmdList.GetCommandListType())
					{
					case dx12::CommandListType::Direct:
					{
						d3dCmdList->SetGraphicsRootDescriptorTable(rootIdx, m_gpuDescriptorHeapGPUBase);
					}
					break;
					case dx12::CommandListType::Compute:
					{
						d3dCmdList->SetComputeRootDescriptorTable(rootIdx, m_gpuDescriptorHeapGPUBase);
					}
					break;
					default:
						SEAssertF("Invalid root signature type");
					}

					// Increment our stack pointers:
					m_gpuDescriptorHeapCPUBase.ptr += tableSize;
					m_gpuDescriptorHeapGPUBase.ptr += tableSize;

					// Flip the dirty bit now that we've updated the GPU-visible descriptor table data:
					m_dirtyDescriptorTableIdxBitmask ^= rootIdxBit;
				}	
			}
		}
	}


	D3D12_GPU_DESCRIPTOR_HANDLE GPUDescriptorHeap::CommitToGPUVisibleHeap(
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> const& src)
	{
		SEAssert(m_gpuDescriptorHeapCPUBase.ptr + m_elementSize <=
			m_gpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + (m_numDescriptors * m_elementSize),
			"Out of bounds CPU destination. Consider increasing m_numDescriptors");

		SEAssert(m_gpuDescriptorHeapGPUBase.ptr + m_elementSize <=
			m_gpuDescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + (m_numDescriptors * m_elementSize),
			"Out of bounds GPU destination. Consider increasing m_numDescriptors");

		const uint32_t numSrcDescriptors = util::CheckedCast<uint32_t>(src.size());

		// Note: Our source descriptors are not contiguous, but our destination descriptors are (as they're on the stack)
		m_deviceCache->CopyDescriptors(
			1,									// UINT NumDestDescriptorRanges
			&m_gpuDescriptorHeapCPUBase,	// const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptorRangeStarts
			&numSrcDescriptors,					// const UINT* pDestDescriptorRangeSizes
			numSrcDescriptors,					// UINT NumSrcDescriptorRanges
			src.data(),							// const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorRangeStarts
			nullptr,							// const UINT* pSrcDescriptorRangeSizes
			m_heapType							// D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType
		);

		const D3D12_GPU_DESCRIPTOR_HANDLE destination = m_gpuDescriptorHeapGPUBase;

		const uint64_t offset = numSrcDescriptors * m_elementSize;

		m_gpuDescriptorHeapCPUBase.ptr += offset;
		m_gpuDescriptorHeapGPUBase.ptr += offset;

		return destination;
	}


	static void CommitInlineDescriptorsHelper(
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
		for (uint32_t rootIdx = 0; rootIdx < GPUDescriptorHeap::k_totalRootSigEntries; rootIdx++)
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

	
	void GPUDescriptorHeap::CommitInlineDescriptors(dx12::CommandList& cmdList)
	{
#if defined(_DEBUG)
		if (m_unsetInlineDescriptors != 0) // Catch unset descriptors
		{
			std::string unsetInlineDescriptorNames;

			std::vector<RootSignature::RootParameter> const& rootParams = m_currentRootSig->GetRootSignatureEntries();
			for (auto const& rootParam : rootParams)
			{
				if (m_unsetInlineDescriptors & (1 << rootParam.m_index))
				{
					unsetInlineDescriptorNames += m_currentRootSig->DebugGetNameFromRootParamIdx(rootParam.m_index) + " ";
				}
			}
			
			SEAssertF(std::format("An inline descriptor has not been set. Shader access will result in undefined "
				"behavior: {}",
				unsetInlineDescriptorNames).c_str());
		}
#endif

		ID3D12GraphicsCommandList* d3dCmdList = cmdList.GetD3DCommandList().Get();
		const dx12::CommandListType commandListType = cmdList.GetCommandListType();

		for (uint8_t inlineRootType = 0; inlineRootType < InlineRootType_Count; inlineRootType++)
		{
			CommitInlineDescriptorsHelper(
				d3dCmdList,
				commandListType,
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
		for (uint32_t i = 0; i < k_totalRootSigEntries; i++)
		{
			const uint32_t bitmask = (1 << i);
			if (m_dirtyDescriptorTableIdxBitmask & bitmask)
			{
				count += m_cpuDescriptorHeapCacheLocations[i].m_numElements;

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