// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "MeshPrimitive_DX12.h"
#include "VertexStream_DX12.h"


namespace dx12
{
	MeshPrimitive::PlatformParams::PlatformParams(re::MeshPrimitive& meshPrimitive)
	{
		// Note: SaberEngine does not support triangle fans or line loops (and neither does DX12)

		switch (meshPrimitive.GetMeshParams().m_topologyMode)
		{
		case re::MeshPrimitive::TopologyMode::PointList:
		{
			m_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
		}
		break;
		case re::MeshPrimitive::TopologyMode::LineList:
		{
			m_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		}
		break;
		case re::MeshPrimitive::TopologyMode::LineStrip:
		{
			m_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
		}
		break;
		case re::MeshPrimitive::TopologyMode::TriangleList:
		{
			m_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
		break;
		case re::MeshPrimitive::TopologyMode::TriangleStrip:
		{
			m_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		}
		break;
		case re::MeshPrimitive::TopologyMode::LineListAdjacency:
		{
			m_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;

		}
		break;
		case re::MeshPrimitive::TopologyMode::LineStripAdjacency:
		{
			m_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;

		}
		break;
		case re::MeshPrimitive::TopologyMode::TriangleListAdjacency:
		{
			m_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;

		}
		break;
		case re::MeshPrimitive::TopologyMode::TriangleStripAdjacency:
		{
			m_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
		}
		break;
		default:
			SEAssertF("Invalid mesh draw mode");
			m_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
	}


	void MeshPrimitive::Create(
		re::MeshPrimitive& meshPrimitive, 
		ID3D12GraphicsCommandList2* copyCommandList, 
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& intermediateResources)
	{
		// Create and enable our vertex buffers
		for (size_t i = 0; i < re::MeshPrimitive::Slot_Count; i++)
		{
			const re::MeshPrimitive::Slot slot = static_cast<re::MeshPrimitive::Slot>(i);
			if (meshPrimitive.GetVertexStream(slot))
			{
				dx12::VertexStream::Create(*meshPrimitive.GetVertexStream(slot), copyCommandList, intermediateResources);
			}
		}
	}


	void MeshPrimitive::Destroy(re::MeshPrimitive& meshPrimitive)
	{
		dx12::MeshPrimitive::PlatformParams* meshPlatParams = 
			meshPrimitive.GetPlatformParams()->As<dx12::MeshPrimitive::PlatformParams*>();

		meshPlatParams->m_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	}
}