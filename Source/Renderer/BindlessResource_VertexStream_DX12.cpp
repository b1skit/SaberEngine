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
	std::function<ResourceHandle(void)> IVertexStreamResource::GetRegistrationCallback(
		re::VertexBufferInput const& vertexBufferInput)
	{
		// Note: We intentionally capture the vertexBufferInput by value here
		auto RegisterVertexStream = [vertexBufferInput]() -> ResourceHandle
			{
				dx12::Context* context = re::Context::GetAs<dx12::Context*>();

				re::BindlessResourceManager& brm = context->GetBindlessResourceManager();

				switch (vertexBufferInput.GetStream()->GetType())
				{
				case gr::VertexStream::Position:
				{
					return brm.RegisterResource<re::VertexStreamResource_Position>(
						std::make_unique<re::VertexStreamResource_Position>(vertexBufferInput));
				}
				break;
				case gr::VertexStream::Normal:
				{
					return brm.RegisterResource<re::VertexStreamResource_Normal>(
						std::make_unique<re::VertexStreamResource_Normal>(vertexBufferInput));
				}
				break;
				case gr::VertexStream::Tangent:
				{
					return brm.RegisterResource<re::VertexStreamResource_Tangent>(
						std::make_unique<re::VertexStreamResource_Tangent>(vertexBufferInput));
				}
				break;
				case gr::VertexStream::TexCoord:
				{
					return brm.RegisterResource<re::VertexStreamResource_TexCoord>(
							std::make_unique<re::VertexStreamResource_TexCoord>(vertexBufferInput));
				}
				break;
				case gr::VertexStream::Color:
				{
					return brm.RegisterResource<re::VertexStreamResource_Color>(
							std::make_unique<re::VertexStreamResource_Color>(vertexBufferInput));
				}
				break;
				case gr::VertexStream::Index:
				{
					return brm.RegisterResource<re::VertexStreamResource_Index>(
						std::make_unique<re::VertexStreamResource_Index>(vertexBufferInput));
				}
				break;
				default: SEAssertF("Invalid vertex stream type");
				}
			};

		return RegisterVertexStream;
	}


	std::function<void(ResourceHandle&)> IVertexStreamResource::GetUnregistrationCallback(
		gr::VertexStream::Type streamType)
	{
		auto UnregisterVertexStream = [streamType](ResourceHandle& resourceHandle)
			{
				dx12::Context* context = re::Context::GetAs<dx12::Context*>();

				re::BindlessResourceManager& brm = context->GetBindlessResourceManager();

				const uint64_t frameNum = re::RenderManager::Get()->GetCurrentRenderFrameNum();

				switch (streamType)
				{
				case gr::VertexStream::Position:
				{
					brm.UnregisterResource<re::VertexStreamResource_Position>(resourceHandle, frameNum);
				}
				break;
				case gr::VertexStream::Normal:
				{
					brm.UnregisterResource<re::VertexStreamResource_Normal>(resourceHandle, frameNum);
				}
				break;
				case gr::VertexStream::Tangent:
				{
					brm.UnregisterResource<re::VertexStreamResource_Tangent>(resourceHandle, frameNum);
				}
				break;
				case gr::VertexStream::TexCoord:
				{
					brm.UnregisterResource<re::VertexStreamResource_TexCoord>(resourceHandle, frameNum);
				}
				break;
				case gr::VertexStream::Color:
				{
					brm.UnregisterResource<re::VertexStreamResource_Color>(resourceHandle, frameNum);
				}
				break;
				case gr::VertexStream::Index:
				{
					brm.UnregisterResource<re::VertexStreamResource_Index>(resourceHandle, frameNum);
				}
				break;
				default: SEAssertF("Invalid vertex stream type");
				}
			};

		return UnregisterVertexStream;
	}


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

		dx12::BindlessResourceManager::PlatformParams* brmPlatParams =
			resourceSet.GetBindlessResourceManager()->GetPlatformParams()->As<dx12::BindlessResourceManager::PlatformParams*>();

		dx12::RootSignature::RootParameter const* tableRootParam =
			brmPlatParams->m_rootSignature->GetRootSignatureEntry(resourceSet.GetShaderName());
		SEAssert(tableRootParam->m_type == dx12::RootSignature::RootParameter::Type::DescriptorTable,
			"Unexpected root parameter type");

		re::IVertexStreamResource const* vertexStreamResource =
			dynamic_cast<re::IVertexStreamResource const*>(&resource);
		SEAssert(vertexStreamResource, "Failed to cast to IVertexStreamResource");

		re::Buffer const* streamBuffer = vertexStreamResource->m_vertexBufferInput.GetBuffer();

		dx12::Buffer::PlatformParams const* streamBufferPlatParams =
			streamBuffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams const*>();

		re::BufferView::BufferType bufferTypeView = re::BufferView::BufferType{
			.m_firstElement = 0,
			.m_numElements = vertexStreamResource->m_vertexBufferInput.GetStream()->GetNumElements(),
			.m_structuredByteStride =
				re::DataTypeToByteStride(vertexStreamResource->m_vertexBufferInput.GetStream()->GetDataType()),
		};

		D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle{};

		switch (tableRootParam->m_tableEntry.m_type)
		{
		case dx12::RootSignature::DescriptorType::SRV:
		{
			descriptorHandle = dx12::Buffer::GetSRV(streamBuffer, bufferTypeView);
		}
		break;
		case dx12::RootSignature::DescriptorType::UAV:
		{
			descriptorHandle = dx12::Buffer::GetUAV(streamBuffer, bufferTypeView);
		}
		break;
		case dx12::RootSignature::DescriptorType::CBV:
		{
			descriptorHandle = dx12::Buffer::GetCBV(streamBuffer, bufferTypeView);
		}
		break;
		default: SEAssertF("Invalid descriptor type");
		}

		SEAssert(descriptorOutByteSize == sizeof(D3D12_CPU_DESCRIPTOR_HANDLE), "Invalid destination size");
		memcpy(descriptorOut, &descriptorHandle, descriptorOutByteSize);
	}


	// ---


	void VertexStreamResourceSet::PopulateRootSignatureDesc(
		re::IBindlessResourceSet const& resourceSet,
		void* destDescriptorRangeCreateDesc)
	{
		SEAssert(destDescriptorRangeCreateDesc,
			"Invalid destination parameters received");

		dx12::RootSignature::DescriptorRangeCreateDesc* descriptorRangeCreateDesc = 
			static_cast<dx12::RootSignature::DescriptorRangeCreateDesc*>(destDescriptorRangeCreateDesc);

		descriptorRangeCreateDesc->m_shaderName = resourceSet.GetShaderName();

		descriptorRangeCreateDesc->m_rangeDesc = D3D12_DESCRIPTOR_RANGE1{
			.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			.NumDescriptors = resourceSet.GetMaxResourceCount(),
			.BaseShaderRegister = 0,
			.RegisterSpace = resourceSet.GetRegisterSpace(),
			.OffsetInDescriptorsFromTableStart = 0,
		};

		re::IVertexStreamResourceSet const* vertexStreamResourceSet =
			dynamic_cast<re::IVertexStreamResourceSet const*>(&resourceSet);

		descriptorRangeCreateDesc->m_srvDesc.m_format = dx12::DataTypeToDXGI_FORMAT(
			vertexStreamResourceSet->GetStreamDataType(), false);

		descriptorRangeCreateDesc->m_srvDesc.m_viewDimension = D3D12_SRV_DIMENSION_BUFFER;
	}
}