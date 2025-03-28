// © 2025 Adam Badke. All rights reserved.
#include "BindlessResourceManager_DX12.h"
#include "BindlessResource_VertexStream_DX12.h"
#include "BindlessResource_VertexStream.h"
#include "Buffer_DX12.h"
#include "Context_DX12.h"
#include "EnumTypes.h"
#include "EnumTypes_DX12.h"

#include "Core/Assert.h"

#include <d3d12.h>


namespace dx12
{
	void IVertexStreamResource::GetPlatformResource(
		re::IBindlessResource const& resource, void* dest, size_t destByteSize)
	{
		SEAssert(dest && destByteSize, "Invalid args received");
		
		re::IVertexStreamResource const* vertexStreamResource = 
			dynamic_cast<re::IVertexStreamResource const*>(&resource);
		SEAssert(vertexStreamResource, "Failed to cast to IVertexStreamResource");

		dx12::Buffer::PlatformParams* streamBufferPlatParams = 
			vertexStreamResource->m_vertexBufferInput.GetBuffer()->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

		SEAssert(streamBufferPlatParams->m_resolvedGPUResource, "Vertex stream buffer resolved resource is null");
		
		SEAssert(destByteSize == sizeof(ID3D12Resource*), "Invalid destination size");

		memcpy(dest, &streamBufferPlatParams->m_resolvedGPUResource, destByteSize);
	}


	void IVertexStreamResource::GetDescriptor(
		re::IBindlessResourceSet const& resourceSet,
		re::IBindlessResource const& resource,
		void* descriptorOut,
		size_t descriptorOutByteSize)
	{
		SEAssert(descriptorOut && descriptorOutByteSize, "Invalid params received");

		re::IVertexStreamResource const* vertexStreamResource =
			dynamic_cast<re::IVertexStreamResource const*>(&resource);
		SEAssert(vertexStreamResource, "Failed to cast to IVertexStreamResource");

		// Vertex streams are (currently) always attached as SRVs:
		D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = dx12::Buffer::GetSRV(
			vertexStreamResource->m_vertexBufferInput.GetBuffer(), 
			re::BufferView::BufferType{
				.m_firstElement = 0,
				.m_numElements = vertexStreamResource->m_vertexBufferInput.GetStream()->GetNumElements(),
				.m_structuredByteStride =
					re::DataTypeToByteStride(vertexStreamResource->m_vertexBufferInput.GetStream()->GetDataType()),
			});

		SEAssert(descriptorOutByteSize == sizeof(D3D12_CPU_DESCRIPTOR_HANDLE), "Invalid destination size");
		memcpy(descriptorOut, &descriptorHandle, descriptorOutByteSize);
	}


	// ---


	void VertexStreamResourceSet::GetNullDescriptor(
		re::IBindlessResourceSet const& resourceSet, void* dest, size_t destByteSize)
	{
		re::IVertexStreamResourceSet const* vertexStreamResourceSet =
			dynamic_cast<re::IVertexStreamResourceSet const*>(&resourceSet);
		SEAssert(vertexStreamResourceSet, "Failed to cast to re::IVertexStreamResourceSet");

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		// Vertex streams are (currently) always attached as SRVs:
		D3D12_CPU_DESCRIPTOR_HANDLE const& result = context->GetNullSRVDescriptor(
			D3D12_SRV_DIMENSION_BUFFER,
			dx12::DataTypeToDXGI_FORMAT(vertexStreamResourceSet->GetStreamDataType(), false)).GetBaseDescriptor();

		SEAssert(destByteSize == sizeof(D3D12_CPU_DESCRIPTOR_HANDLE), "Unexpected destination byte size");

		memcpy(dest, &result, destByteSize);
	}


	void VertexStreamResourceSet::GetResourceUsageState(
		re::IBindlessResourceSet const& resourceSet, void* dest, size_t destByteSize)
	{
		SEAssert(dest && destByteSize == sizeof(D3D12_RESOURCE_STATES), "Invalid or unexpected destination params");

		constexpr D3D12_RESOURCE_STATES k_defaultVertexStreamState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		memcpy(dest, &k_defaultVertexStreamState, destByteSize);
	}
}