// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <d3d12.h>
#include <dxgi1_6.h>

#include "Context_DX12.h"
#include "VertexStream_DX12.h"
#include "MeshPrimitive.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	std::unique_ptr<re::VertexStream::PlatformParams> VertexStream::CreatePlatformParams(re::VertexStream::StreamType type)
	{
		switch (type)
		{
		case re::VertexStream::StreamType::Index:
		{
			return std::make_unique<dx12::VertexStream::PlatformParams_Index>();
		}
		break;
		default:
		{
			return std::make_unique<dx12::VertexStream::PlatformParams_Vertex>();
		}
		}
	}


	void VertexStream::Create(re::VertexStream& vertexStream, re::MeshPrimitive::Slot)
	{
		LOG_ERROR("TODO: Implement dx12::VertexStream::Create");
	}


	void VertexStream::Destroy(re::VertexStream&)
	{
		#pragma message("TODO: Implement dx12::VertexStream::Destroy");
	}
}