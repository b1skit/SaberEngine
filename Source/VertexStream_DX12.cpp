// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <d3d12.h>
#include <dxgi1_6.h>

#include "Context_DX12.h"
#include "MeshPrimitive.h"
#include "RenderManager.h"
#include "VertexStream_DX12.h"

using Microsoft::WRL::ComPtr;


namespace
{
	DXGI_FORMAT GetStreamFormat(re::VertexStream const& stream)
	{
		switch (stream.GetNumComponents())
		{
		case 1:
		{
			switch (stream.GetDataType())
			{
			case re::VertexStream::DataType::Float:
			{
				SEAssert("Normalized 32 bit float types are not supported", !stream.DoNormalize());
				return DXGI_FORMAT_R32_FLOAT;
			}
			break;
			case re::VertexStream::DataType::UInt:
			{
				SEAssert("Normalized 32 bit uint types are not supported", !stream.DoNormalize());
				return DXGI_FORMAT_R32_UINT;
			}
			break;
			case re::VertexStream::DataType::UByte:
			{
				return stream.DoNormalize() ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8_UINT;
			}
			break;
			default:
				SEAssertF("Invalid data type");
			}
		}
		break;
		case 2:
		{
			switch (stream.GetDataType())
			{
			case re::VertexStream::DataType::Float:
			{
				SEAssert("Normalized 32 bit float types are not supported", !stream.DoNormalize());
				return DXGI_FORMAT_R32G32_FLOAT;
			}
			break;
			case re::VertexStream::DataType::UInt:
			{
				SEAssert("Normalized 32 bit uint types are not supported", !stream.DoNormalize());
				return DXGI_FORMAT_R32G32_UINT;
			}
			break;
			case re::VertexStream::DataType::UByte:
			{
				return stream.DoNormalize() ? DXGI_FORMAT_R8G8_UNORM : DXGI_FORMAT_R8G8_UINT;
			}
			break;
			default:
				SEAssertF("Invalid data type");
			}
		}
		case 3:
		{
			switch (stream.GetDataType())
			{
			case re::VertexStream::DataType::Float:
			{
				SEAssert("Normalized 32 bit float types are not supported", !stream.DoNormalize());
				return DXGI_FORMAT_R32G32B32_FLOAT;
			}
			break;
			case re::VertexStream::DataType::UInt:
			{
				SEAssert("Normalized 32 bit uint types are not supported", !stream.DoNormalize());
				return DXGI_FORMAT_R32G32B32_UINT;
			}
			break;
			case re::VertexStream::DataType::UByte:
			{
				SEAssertF("8-bit, 3-channel unsigned byte types are not supported");
			}
			break;
			default:
				SEAssertF("Invalid data type");
			}
		}
		break;
		case 4:
		{
			switch (stream.GetDataType())
			{
			case re::VertexStream::DataType::Float:
			{
				SEAssert("Normalized 32 bit float types are not supported", !stream.DoNormalize());
				return DXGI_FORMAT_R32G32B32A32_FLOAT;
			}
			break;
			case re::VertexStream::DataType::UInt:
			{
				SEAssert("Normalized 32 bit uint types are not supported", !stream.DoNormalize());
				return DXGI_FORMAT_R32G32B32A32_UINT;
			}
			break;
			case re::VertexStream::DataType::UByte:
			{
				return stream.DoNormalize() ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UINT;
			}
			break;
			default:
				SEAssertF("Invalid data type");
			}
		}
		break;
		default:
			SEAssertF("Invalid number of stream components");
		}

		SEAssertF("Cannot compute stream format");
		return DXGI_FORMAT_FORCE_UINT;
	}
}

namespace dx12
{
	std::unique_ptr<re::VertexStream::PlatformParams> VertexStream::CreatePlatformParams(
		re::VertexStream const& stream, re::VertexStream::StreamType type)
	{
		switch (type)
		{
		case re::VertexStream::StreamType::Index:
		{
			return std::make_unique<dx12::VertexStream::PlatformParams_Index>(stream);
		}
		break;
		default:
		{
			return std::make_unique<dx12::VertexStream::PlatformParams_Vertex>(stream);
		}
		}
	}

	VertexStream::PlatformParams::PlatformParams(re::VertexStream const& stream, re::VertexStream::StreamType type)
		: m_type(type)
		, m_intermediateBufferResource(nullptr)
		, m_bufferResource(nullptr)
	{
		m_format = GetStreamFormat(stream);
	}


	dx12::VertexStream::PlatformParams_Vertex::PlatformParams_Vertex(re::VertexStream const& stream)
		: dx12::VertexStream::PlatformParams::PlatformParams(stream, re::VertexStream::StreamType::Vertex)
		, m_vertexBufferView{}
	{
	}


	dx12::VertexStream::PlatformParams_Index::PlatformParams_Index(re::VertexStream const& stream)
		: dx12::VertexStream::PlatformParams::PlatformParams(stream, re::VertexStream::StreamType::Index)
		, m_indexBufferView{}
	{
	}


	/******************************************************************************************************************/


	void VertexStream::Create(re::VertexStream& stream, ComPtr<ID3D12GraphicsCommandList2> commandList)
	{
		const D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
		const CD3DX12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		const size_t bufferSize = stream.GetTotalDataByteSize();
		const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);

		dx12::VertexStream::PlatformParams* const streamPlatformParams =
			dynamic_cast<dx12::VertexStream::PlatformParams*>(stream.GetPlatformParams());

		// Get our interface pointers from the stream platform params:
		ID3D12Resource** intermediateBufferResource = nullptr;
		ID3D12Resource** destBufferResource = nullptr;
		switch (streamPlatformParams->m_type)
		{
		case re::VertexStream::StreamType::Index:
		{
			dx12::VertexStream::PlatformParams_Index* const indexPlatformParams =
				dynamic_cast<dx12::VertexStream::PlatformParams_Index*>(streamPlatformParams);

			intermediateBufferResource = &indexPlatformParams->m_intermediateBufferResource;
			destBufferResource = &indexPlatformParams->m_bufferResource;
		}
		break;
		default:
		{
			dx12::VertexStream::PlatformParams_Vertex* const vertPlatformParams =
				dynamic_cast<dx12::VertexStream::PlatformParams_Vertex*>(streamPlatformParams);
			
			intermediateBufferResource = &vertPlatformParams->m_intermediateBufferResource;
			destBufferResource = &vertPlatformParams->m_bufferResource;
		}
		}

		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(re::RenderManager::Get()->GetContext().GetPlatformParams());
		Microsoft::WRL::ComPtr<ID3D12Device2> device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		// Create a committed resource for the GPU resource in a default heap:
		HRESULT hr = device->CreateCommittedResource(
			&defaultHeapProperties,				// Heap properties
			D3D12_HEAP_FLAG_NONE,				// Heap flags
			&resourceDesc,						// Resource description
			D3D12_RESOURCE_STATE_COPY_DEST,		// Initial resource state
			nullptr,							// Clear value: nullptr as these are not texture resources
			IID_PPV_ARGS(destBufferResource));	// RefIID and interface pointer


		// Create an committed resource for the upload:
		const CD3DX12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RESOURCE_DESC committedresourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

		hr = device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&committedresourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(intermediateBufferResource));

		// Populate the subresource:
		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = stream.GetData();
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		::UpdateSubresources(
			commandList.Get(),
			*destBufferResource,			// Destination resource
			*intermediateBufferResource,	// Intermediate resource
			0,								// Index of 1st subresource in the resource
			0,								// Number of subresources in the resource.
			1,								// Required byte size for the update
			&subresourceData);

		// Create the resource view:
		switch (streamPlatformParams->m_type)
		{
		case re::VertexStream::StreamType::Index:
		{
			dx12::VertexStream::PlatformParams_Index* const indexPlatformParams =
				dynamic_cast<dx12::VertexStream::PlatformParams_Index*>(streamPlatformParams);

			// Create the index buffer view:
			indexPlatformParams->m_indexBufferView.BufferLocation = 
				indexPlatformParams->m_bufferResource->GetGPUVirtualAddress();

			indexPlatformParams->m_indexBufferView.Format = indexPlatformParams->m_format;
			indexPlatformParams->m_indexBufferView.SizeInBytes = stream.GetTotalDataByteSize();
		}
		break;
		default:
		{
			dx12::VertexStream::PlatformParams_Vertex* const vertPlatformParams =
				dynamic_cast<dx12::VertexStream::PlatformParams_Vertex*>(streamPlatformParams);

			// Create the vertex buffer view:
			vertPlatformParams->m_vertexBufferView.BufferLocation = 
				vertPlatformParams->m_bufferResource->GetGPUVirtualAddress();

			vertPlatformParams->m_vertexBufferView.SizeInBytes = stream.GetTotalDataByteSize();
			vertPlatformParams->m_vertexBufferView.StrideInBytes = stream.GetElementByteSize();
		}
		}
	}


	void VertexStream::Destroy(re::VertexStream& stream)
	{
		dx12::VertexStream::PlatformParams* const streamPlatformParams =
			dynamic_cast<dx12::VertexStream::PlatformParams*>(stream.GetPlatformParams());

		switch (streamPlatformParams->m_type)
		{
		case re::VertexStream::StreamType::Index:
		{
			dx12::VertexStream::PlatformParams_Index* const indexPlatformParams =
				dynamic_cast<dx12::VertexStream::PlatformParams_Index*>(streamPlatformParams);

			indexPlatformParams->m_indexBufferView = { 0 };
		}
		break;
		default:
		{
			dx12::VertexStream::PlatformParams_Vertex* const vertPlatformParams =
				dynamic_cast<dx12::VertexStream::PlatformParams_Vertex*>(streamPlatformParams);

			vertPlatformParams->m_vertexBufferView = { 0 };
		}
		}

		streamPlatformParams->m_type = re::VertexStream::StreamType::StreamType_Count;
		streamPlatformParams->m_intermediateBufferResource = nullptr;
		streamPlatformParams->m_bufferResource = nullptr;
		streamPlatformParams->m_format = DXGI_FORMAT::DXGI_FORMAT_FORCE_UINT;
	}
}