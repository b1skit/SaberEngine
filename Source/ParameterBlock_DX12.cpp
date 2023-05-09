// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Context_DX12.h"
#include "DebugConfiguration.h"
#include "Debug_DX12.h"
#include "MathUtils.h"
#include "ParameterBlock_DX12.h"
#include "RenderManager.h"


namespace
{
	constexpr size_t k_CBVSizeFactor = 256; // CBV sizes must be in multiples of 256B
	constexpr size_t k_StructuredBufferSizeFactor = 64 * 1024; // Structured buffer sizes must be in multiples of 64KB
}


namespace dx12
{
	void ParameterBlock::Create(re::ParameterBlock& paramBlock)
	{
		dx12::ParameterBlock::PlatformParams* params =
			paramBlock.GetPlatformParams()->As<dx12::ParameterBlock::PlatformParams*>();

		// TODO: ParameterBlock should self-register for API creation when it is first created, instead of checking this
		// every time (it's already using an object factory!)
		if (params->m_isCreated)
		{
			return;
		}
		params->m_isCreated = true;

		re::Context const& context = re::RenderManager::Get()->GetContext();
		dx12::Context::PlatformParams* ctxPlatParams = context.GetPlatformParams()->As<dx12::Context::PlatformParams*>();
		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		size_t size = 0;

		switch(params->m_dataType)
		{
		case re::ParameterBlock::PBDataType::SingleElement:
		{
			// We must allocate CBVs in multiples of 256B
			size = util::RoundUpToNearestMultiple<size_t>(paramBlock.GetSize(), k_CBVSizeFactor);

			// Allocate a cpu-visible descriptor to hold our view:
			params->m_cpuDescAllocation = std::move(
				ctxPlatParams->m_cpuDescriptorHeapMgrs[dx12::Context::CPUDescriptorHeapType::CBV_SRV_UAV].Allocate(1));
		}
		break;
		case re::ParameterBlock::PBDataType::Array:
		{
			// We must allocate SRVs in multiples of 64KB
			size = util::RoundUpToNearestMultiple<size_t>(paramBlock.GetSize(), k_StructuredBufferSizeFactor);

			// Allocate a cpu-visible descriptor to hold our view:
			params->m_cpuDescAllocation = std::move(
				ctxPlatParams->m_cpuDescriptorHeapMgrs[dx12::Context::CPUDescriptorHeapType::CBV_SRV_UAV].Allocate(
					params->m_numElements));
		}
		break;
		case re::ParameterBlock::PBDataType::PBDataType_Count:
		default:
			SEAssertF("Invalid parameter block data type");
		}


		// Our parameter blocks live in the upload heap, as they're typically small and updated frequently. 
		// No point copying them to VRAM, for now
		const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

		HRESULT hr = device->CreateCommittedResource(
			&heapProperties,					// this heap will be used to upload the constant buffer data
			D3D12_HEAP_FLAG_NONE,				// Flags
			&resourceDesc,						// Size of the resource heap: Alignment must be a multiple of 64KB for single-textures and constant buffers
			D3D12_RESOURCE_STATE_GENERIC_READ,	// Will be data that is read from: Keep it in the generic read state
			nullptr,							// Optimized clear value: None for constant buffers
			IID_PPV_ARGS(&params->m_resource));
		CheckHResult(hr, "Failed to create committed resource");
		// Note: No need to initialize the heap; We created with D3D12_HEAP_FLAG_NONE so the buffer was zeroed

		params->m_resource->SetName(paramBlock.GetWName().c_str());

		// Create the appropriate resource view:
		switch (params->m_dataType)
		{
		case re::ParameterBlock::PBDataType::SingleElement:
		{
			// Create the constant buffer view:
			D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc;
			constantBufferViewDesc.BufferLocation = params->m_resource->GetGPUVirtualAddress();
			constantBufferViewDesc.SizeInBytes = static_cast<uint32_t>(size);

			device->CreateConstantBufferView(
				&constantBufferViewDesc,
				params->m_cpuDescAllocation.GetBaseDescriptor());
		}
		break;
		case re::ParameterBlock::PBDataType::Array:
		{
			SEAssert("Size must be equally divisible by the number of elements",
				paramBlock.GetSize() % params->m_numElements == 0);
			SEAssert("Maximum offset of 1024 allowed into an SRV", params->m_numElements <= 1024);

			// Create the SRV:
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.Format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer = D3D12_BUFFER_SRV{
				.FirstElement = 0,
				.NumElements = params->m_numElements,
				.StructureByteStride = static_cast<uint32_t>(paramBlock.GetStride()),
				.Flags = D3D12_BUFFER_SRV_FLAGS::D3D12_BUFFER_SRV_FLAG_NONE };

			device->CreateShaderResourceView(
				params->m_resource.Get(),
				&srvDesc,
				params->m_cpuDescAllocation.GetBaseDescriptor());


			// TODO: We currently map the PB array data here, because SingleFrame PBs don't ever get an Update() call
			// -> PBs should register with the RenderManager API creation queue?
			{
				// Get a CPU pointer to the subresource (i.e subresource 0)
				void* cpuVisibleData = nullptr;
				CD3DX12_RANGE readRange(0, 0);    // We do not intend to read from this resource on the CPU (end <= begin)
				HRESULT hr = params->m_resource->Map(
					0,						// Subresource
					&readRange,
					&cpuVisibleData);
				CheckHResult(hr, "ParameterBlock: Failed to map committed resource");

				// Get the PB data:
				void const* srcData = nullptr;
				size_t srcSize = 0; // This will be <= the allocated size, as we rounded up when creating the resource
				paramBlock.GetDataAndSize(srcData, srcSize);

				// Set the data in the cpu-visible heap:
				memcpy(cpuVisibleData, srcData, srcSize);

				// Release the map
				D3D12_RANGE writtenRange{ 0, srcSize };
				params->m_resource->Unmap(
					0,
					&writtenRange); // Unmap range: The region the CPU may have modified. Nullptr = entire subresource
			}
		}
		break;
		case re::ParameterBlock::PBDataType::PBDataType_Count:
		default:
			SEAssertF("Invalid parameter block data type");
		}
	}


	void ParameterBlock::Update(re::ParameterBlock& paramBlock)
	{
		// Ensure the PB is created before we attempt to update it
		dx12::ParameterBlock::Create(paramBlock);

		dx12::ParameterBlock::PlatformParams* params =
			paramBlock.GetPlatformParams()->As<dx12::ParameterBlock::PlatformParams*>();

		switch (params->m_dataType)
		{
		case re::ParameterBlock::PBDataType::SingleElement:
		{
			// Get a CPU pointer to the subresource (i.e subresource 0) in our constant buffer resource
			void* cpuVisibleData = nullptr;
			CD3DX12_RANGE readRange(0, 0);    // We do not intend to read from this resource on the CPU (end <= begin)
			HRESULT hr = params->m_resource->Map(
				0,						// Subresource
				&readRange,
				&cpuVisibleData);
			CheckHResult(hr, "ParameterBlock: Failed to map committed resource");

			// Get the PB data:
			void const* srcData = nullptr;
			size_t srcSize = 0; // This will be <= the allocated size, as we rounded up when creating the resource
			paramBlock.GetDataAndSize(srcData, srcSize);

			// Set the data in the cpu-visible heap:
			memcpy(cpuVisibleData, srcData, srcSize);

			// Release the map
			D3D12_RANGE writtenRange{ 0, srcSize };
			params->m_resource->Unmap(
				0,
				&writtenRange); // Unmap range: The region the CPU may have modified. Nullptr = entire subresource
		}
		break;
		case re::ParameterBlock::PBDataType::Array: // Currently, Array PB data is buffered during creation
		case re::ParameterBlock::PBDataType::PBDataType_Count:
		default:
			SEAssertF("Invalid parameter block data type");
		}
	}


	void ParameterBlock::Destroy(re::ParameterBlock& paramBlock)
	{
		dx12::ParameterBlock::PlatformParams* params =
			paramBlock.GetPlatformParams()->As<dx12::ParameterBlock::PlatformParams*>();

		if (!params->m_isCreated)
		{
			return;
		}

		params->m_cpuDescAllocation.Free(0);
		params->m_resource = nullptr;
		params->m_isCreated = false;
		params->m_dataType = re::ParameterBlock::PBDataType::PBDataType_Count;
		params->m_numElements = 0;
	}
}