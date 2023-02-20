// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include "VertexStream.h"
#include "MeshPrimitive.h"


namespace dx12
{
	class VertexStream
	{
	public:
		struct PlatformParams_Vertex final : public virtual re::VertexStream::PlatformParams
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> m_bufferResource;
			D3D12_VERTEX_BUFFER_VIEW m_bufferView;
		};

		struct PlatformParams_Index final : public virtual re::VertexStream::PlatformParams
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> m_bufferResource;
			D3D12_INDEX_BUFFER_VIEW  m_bufferView;
		};
		
		static std::unique_ptr<re::VertexStream::PlatformParams> CreatePlatformParams(re::VertexStream::StreamType type);

	public:
		static void Create(re::VertexStream& vertexStream, re::MeshPrimitive::Slot);
		static void Destroy(re::VertexStream&);
	};
}