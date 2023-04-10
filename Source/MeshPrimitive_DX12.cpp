// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "MeshPrimitive_DX12.h"
#include "VertexStream_DX12.h"


namespace dx12
{
	MeshPrimitive::PlatformParams::PlatformParams(re::MeshPrimitive& meshPrimitive)
	{
		switch (meshPrimitive.GetMeshParams().m_drawMode)
		{
		case re::MeshPrimitive::DrawMode::Points:
		{
			m_drawMode = D3D_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_POINTLIST;
		}
		break;
		case re::MeshPrimitive::DrawMode::Lines:
		{
			m_drawMode = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		}
		break;
		case re::MeshPrimitive::DrawMode::LineStrip:
		{
			m_drawMode = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
		}
		break;
		case re::MeshPrimitive::DrawMode::LineLoop:
		{
			m_drawMode = D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
			SEAssertF("D3D12 does not support line loops. TODO: Handle this by appending extra vertices in D3D mode");
		}
		break;
		case re::MeshPrimitive::DrawMode::Triangles:
		{
			m_drawMode = D3D_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
		break;
		case re::MeshPrimitive::DrawMode::TriangleStrip:
		{
			m_drawMode = D3D_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		}
		break;
		case re::MeshPrimitive::DrawMode::TriangleFan:
		{
			m_drawMode = D3D_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			SEAssertF("D3D does not support triangle fans since D3D10. TODO: Handle this by converting fans to strips "
				"in D3D mode");
		}
		break;
		case re::MeshPrimitive::DrawMode::DrawMode_Count:
		default:
			SEAssertF("Invalid mesh draw mode");
		}
	}


	void MeshPrimitive::Create(re::MeshPrimitive& meshPrimitive, ID3D12GraphicsCommandList2* commandList)
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