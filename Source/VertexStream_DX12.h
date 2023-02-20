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
		struct PlatformParams : public virtual re::VertexStream::PlatformParams
		{
			PlatformParams(re::VertexStream const&, re::VertexStream::StreamType);
			virtual ~PlatformParams() override = 0;

			re::VertexStream::StreamType m_type;

			Microsoft::WRL::ComPtr<ID3D12Resource> m_intermediateBufferResource;
			Microsoft::WRL::ComPtr<ID3D12Resource> m_bufferResource;

			DXGI_FORMAT m_format;
		};


		struct PlatformParams_Index final : public virtual dx12::VertexStream::PlatformParams
		{
			PlatformParams_Index(re::VertexStream const&);

			D3D12_INDEX_BUFFER_VIEW  m_indexBufferView;
		};


		struct PlatformParams_Vertex final : public virtual dx12::VertexStream::PlatformParams
		{
			PlatformParams_Vertex(re::VertexStream const&);

			D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
		};
		
		
		static std::unique_ptr<re::VertexStream::PlatformParams> CreatePlatformParams(
			re::VertexStream const&, re::VertexStream::StreamType);

	public:
		static void Destroy(re::VertexStream&);

		// DX12-specific functionality
		static void Create(re::VertexStream&, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>);
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline dx12::VertexStream::PlatformParams::~PlatformParams() {};
}