// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include "VertexStream.h"
#include "MeshPrimitive.h"


namespace dx12
{
	class CommandList;


	class VertexStream
	{
	public:
		struct PlatformParams : public re::VertexStream::PlatformParams
		{
			//
		};	
		
		static std::unique_ptr<re::VertexStream::PlatformParams> CreatePlatformParams(re::VertexStream const&);

	public:
		static void Destroy(re::VertexStream const&);

		// DX12-specific functionality
		static void Create(
			re::VertexStream const&, 
			dx12::CommandList* copyCmdList,
			std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&);

		static DXGI_FORMAT GetDXGIStreamFormat(re::DataType, bool isNormalized);
		static DXGI_FORMAT GetDXGIStreamFormat(re::VertexStream const&);
	};
}