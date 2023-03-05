// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>
#include <d3d12.h>

#include "MeshPrimitive.h"


namespace dx12
{
	class MeshPrimitive
	{
	public:
		struct PlatformParams final : public re::MeshPrimitive::PlatformParams
		{
			PlatformParams(re::MeshPrimitive& meshPrimitive);
		};


	public:
		static void Destroy(re::MeshPrimitive&);

		// DX12-specific functionality:
		static void Create(re::MeshPrimitive&, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>);
	};
}