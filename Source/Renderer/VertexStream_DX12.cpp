// © 2022 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "MeshPrimitive.h"
#include "RenderManager.h"
#include "VertexStream_DX12.h"

#include "Core/Util/TextUtils.h"

#include <d3dx12.h>
#include <d3d12.h>
#include <dxgi1_6.h>

using Microsoft::WRL::ComPtr;


namespace dx12
{
	DXGI_FORMAT VertexStream::GetDXGIStreamFormat(re::VertexStream const& stream)
	{
		switch (stream.GetDataType())
		{
		case re::VertexStream::DataType::Float: return DXGI_FORMAT_R32_FLOAT;			
		case re::VertexStream::DataType::Float2: return DXGI_FORMAT_R32G32_FLOAT;
		case re::VertexStream::DataType::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
		case re::VertexStream::DataType::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;

		case re::VertexStream::DataType::Int: return DXGI_FORMAT_R32_SINT;
		case re::VertexStream::DataType::Int2: return DXGI_FORMAT_R32G32_SINT;
		case re::VertexStream::DataType::Int3: return DXGI_FORMAT_R32G32B32_SINT;
		case re::VertexStream::DataType::Int4: return DXGI_FORMAT_R32G32B32A32_SINT;

		case re::VertexStream::DataType::UInt: return DXGI_FORMAT_R32_UINT;
		case re::VertexStream::DataType::UInt2: return DXGI_FORMAT_R32G32_UINT;
		case re::VertexStream::DataType::UInt3: return DXGI_FORMAT_R32G32B32_UINT;
		case re::VertexStream::DataType::UInt4: return DXGI_FORMAT_R32G32B32A32_UINT;

		case re::VertexStream::DataType::Short:
			return stream.DoNormalize() ? DXGI_FORMAT_R16_SNORM : DXGI_FORMAT_R16_SINT;
		case re::VertexStream::DataType::Short2:
			return stream.DoNormalize() ? DXGI_FORMAT_R16G16_SNORM : DXGI_FORMAT_R16G16_SINT;
		case re::VertexStream::DataType::Short4:
			return stream.DoNormalize() ? DXGI_FORMAT_R16G16B16A16_SNORM : DXGI_FORMAT_R16G16B16A16_SINT;

		case re::VertexStream::DataType::UShort: 
			return stream.DoNormalize() ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R16_UINT;
		case re::VertexStream::DataType::UShort2: 
			return stream.DoNormalize() ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R16G16_UINT;
		case re::VertexStream::DataType::UShort4:
			return stream.DoNormalize() ? DXGI_FORMAT_R16G16B16A16_UNORM : DXGI_FORMAT_R16G16B16A16_UINT;

		case re::VertexStream::DataType::Byte:
			return stream.DoNormalize() ? DXGI_FORMAT_R8_SNORM : DXGI_FORMAT_R8_SINT;
		case re::VertexStream::DataType::Byte2:
			return stream.DoNormalize() ? DXGI_FORMAT_R8G8_SNORM : DXGI_FORMAT_R8G8_SINT;
		case re::VertexStream::DataType::Byte4:
			return stream.DoNormalize() ? DXGI_FORMAT_R8G8B8A8_SNORM : DXGI_FORMAT_R8G8B8A8_SINT;

		case re::VertexStream::DataType::UByte:
			return stream.DoNormalize() ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8_UINT;
		case re::VertexStream::DataType::UByte2: 
			return stream.DoNormalize() ? DXGI_FORMAT_R8G8_UNORM : DXGI_FORMAT_R8G8_UINT;
		case re::VertexStream::DataType::UByte4: 
			return stream.DoNormalize() ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UINT;
		default: SEAssertF("Invalid stream data type");
		}

		return DXGI_FORMAT_UNKNOWN; // This should never hapen
	}


	std::unique_ptr<re::VertexStream::PlatformParams> VertexStream::CreatePlatformParams(
		re::VertexStream const& stream, re::VertexStream::Type type)
	{
		switch (type)
		{
		case re::VertexStream::Type::Index:
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

	VertexStream::PlatformParams::PlatformParams(re::VertexStream const& stream)
		: m_bufferResource(nullptr)
	{
		m_format = GetDXGIStreamFormat(stream);
	}


	dx12::VertexStream::PlatformParams_Vertex::PlatformParams_Vertex(re::VertexStream const& stream)
		: dx12::VertexStream::PlatformParams::PlatformParams(stream)
		, m_vertexBufferView{}
	{
	}


	dx12::VertexStream::PlatformParams_Index::PlatformParams_Index(re::VertexStream const& stream)
		: dx12::VertexStream::PlatformParams::PlatformParams(stream)
		, m_indexBufferView{}
	{
	}


	/******************************************************************************************************************/


	void VertexStream::Create(
		re::VertexStream const& stream, 
		dx12::CommandList* copyCmdList,
		std::vector<ComPtr<ID3D12Resource>>& intermediateResources)
	{
		dx12::VertexStream::PlatformParams* streamPlatformParams =
			stream.GetPlatformParams()->As<dx12::VertexStream::PlatformParams*>();

		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();


		const D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
		const CD3DX12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		const size_t bufferSize = stream.GetTotalDataByteSize();
		const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);

		// Create a committed resource for the GPU-visible resource in a default heap:
		HRESULT hr = device->CreateCommittedResource(
			&defaultHeapProperties,				// Heap properties
			D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,	// Heap flags
			&resourceDesc,						// Resource description
			D3D12_RESOURCE_STATE_COMMON,		// Initial resource state
			nullptr,							// Clear value: nullptr as these are not texture resources
			IID_PPV_ARGS(&streamPlatformParams->m_bufferResource));	// RefIID and interface pointer
		CheckHResult(hr, "Failed to create vertex buffer resource");

		std::wstring const& bufferName = util::ToWideString(std::format("{} stream hash {}",
			re::VertexStream::TypeToCStr(stream.GetType()),
			stream.GetDataHash()));

		streamPlatformParams->m_bufferResource->SetName(bufferName.c_str());

		// Create an intermediate upload heap:
		const CD3DX12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RESOURCE_DESC committedresourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

		ComPtr<ID3D12Resource> itermediateBufferResource = nullptr;
		hr = device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
			&committedresourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&itermediateBufferResource));
		CheckHResult(hr, "Failed to create intermediate vertex buffer resource");

		itermediateBufferResource->SetName(L"Vertex stream intermediate buffer");

		// Record the copy:
		copyCmdList->UpdateSubresources(&stream, itermediateBufferResource.Get(), 0);

		// Create the resource view:
		switch (stream.GetType())
		{
		case re::VertexStream::Type::Index:
		{
			dx12::VertexStream::PlatformParams_Index* indexPlatformParams =
				streamPlatformParams->As<dx12::VertexStream::PlatformParams_Index*>();

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
				streamPlatformParams->As<dx12::VertexStream::PlatformParams_Vertex*>();

			// Create the vertex buffer view:
			vertPlatformParams->m_vertexBufferView.BufferLocation = 
				vertPlatformParams->m_bufferResource->GetGPUVirtualAddress();

			vertPlatformParams->m_vertexBufferView.SizeInBytes = stream.GetTotalDataByteSize();
			vertPlatformParams->m_vertexBufferView.StrideInBytes = stream.GetElementByteSize();
		}
		}

		// This will be released once the copy is done
		intermediateResources.emplace_back(itermediateBufferResource);

		// Register the resource with the global resource state tracker:
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		context->GetGlobalResourceStates().RegisterResource(
			streamPlatformParams->m_bufferResource.Get(),
			initialState,
			1);
	}


	void VertexStream::Destroy(re::VertexStream const& stream)
	{
		dx12::VertexStream::PlatformParams* streamPlatformParams =
			stream.GetPlatformParams()->As<dx12::VertexStream::PlatformParams*>();

		switch (stream.GetType())
		{
		case re::VertexStream::Type::Index:
		{
			dx12::VertexStream::PlatformParams_Index* indexPlatformParams =
				streamPlatformParams->As<dx12::VertexStream::PlatformParams_Index*>();

			indexPlatformParams->m_indexBufferView = { 0 };
		}
		break;
		default:
		{
			dx12::VertexStream::PlatformParams_Vertex* const vertPlatformParams =
				streamPlatformParams->As<dx12::VertexStream::PlatformParams_Vertex*>();

			vertPlatformParams->m_vertexBufferView = { 0 };
		}
		}

		// Unregister the resource from the global resource state tracker before we release the ComPtr
		if (streamPlatformParams->m_bufferResource)
		{
			re::Context::GetAs<dx12::Context*>()->GetGlobalResourceStates().UnregisterResource(
				streamPlatformParams->m_bufferResource.Get());
		}

		streamPlatformParams->m_bufferResource = nullptr;
		streamPlatformParams->m_format = DXGI_FORMAT::DXGI_FORMAT_FORCE_UINT;
	}
}