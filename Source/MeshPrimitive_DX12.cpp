// � 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "MeshPrimitive_DX12.h"
#include "VertexStream_DX12.h"


namespace dx12
{
	MeshPrimitive::PlatformParams::PlatformParams(re::MeshPrimitive& meshPrimitive)
	{
	}


	void MeshPrimitive::Create(
		re::MeshPrimitive& meshPrimitive, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList)
	{
		// Create and enable our vertex buffers
		for (size_t i = 0; i < re::MeshPrimitive::Slot_Count; i++)
		{
			const re::MeshPrimitive::Slot slot = static_cast<re::MeshPrimitive::Slot>(i);
			if (meshPrimitive.GetVertexStream(slot))
			{
				dx12::VertexStream::Create(*meshPrimitive.GetVertexStream(slot), commandList);
			}
		}
	}


	void MeshPrimitive::Destroy(re::MeshPrimitive& meshPrimitive)
	{
		#pragma message("TODO: Implement dx12::MeshPrimitive::Destroy")
	}
}