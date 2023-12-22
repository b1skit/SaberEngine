// © 2022 Adam Badke. All rights reserved.
#include "MeshPrimitive.h"
#include "SceneManager.h"


namespace
{
	constexpr char const* DrawModeToCStr(gr::MeshPrimitive::TopologyMode drawMode)
	{
		switch (drawMode)
		{
		case gr::MeshPrimitive::TopologyMode::PointList: return "PointList";
		case gr::MeshPrimitive::TopologyMode::LineList: return "LineList";
		case gr::MeshPrimitive::TopologyMode::LineStrip: return "LineStrip";
		case gr::MeshPrimitive::TopologyMode::TriangleList: return "TriangleList";
		case gr::MeshPrimitive::TopologyMode::TriangleStrip: return "TriangleStrip";
		case gr::MeshPrimitive::TopologyMode::LineListAdjacency: return "LineListAdjacency";
		case gr::MeshPrimitive::TopologyMode::LineStripAdjacency: return "LineStripAdjacency";
		case gr::MeshPrimitive::TopologyMode::TriangleListAdjacency: return "TriangleListAdjacency";
		case gr::MeshPrimitive::TopologyMode::TriangleStripAdjacency: return "TriangleStripAdjacency";
		default: SEAssertF("Invalid draw mode");
		}
		return "INVALID DRAW MODE";
	}


	constexpr char const* SlotToCStr(gr::MeshPrimitive::Slot slot)
	{
		switch (slot)
		{
		case gr::MeshPrimitive::Slot::Position: return "Position";
		case gr::MeshPrimitive::Slot::Normal: return "Normal";
		case gr::MeshPrimitive::Slot::Tangent: return "Tangent";
		case gr::MeshPrimitive::Slot::UV0: return "UV0";
		case gr::MeshPrimitive::Slot::Color: return "Color";
		case gr::MeshPrimitive::Slot::Joints: return "Joints";
		case gr::MeshPrimitive::Slot::Weights: return "Weights";
		default:SEAssertF("Invalid slot index");
		}
		return "INVALID SLOT INDEX";
	}
}

namespace gr
{
	std::shared_ptr<MeshPrimitive> MeshPrimitive::Create(
		std::string const& name,
		std::vector<uint32_t>* indices,
		std::vector<float>& positions,
		std::vector<float>* normals,
		std::vector<float>* tangents,
		std::vector<float>* uv0,
		std::vector<float>* colors,
		std::vector<uint8_t>* joints,
		std::vector<float>* weights,
		gr::MeshPrimitive::MeshPrimitiveParams const& meshParams)
	{
		std::shared_ptr<MeshPrimitive> newMeshPrimitive;
		newMeshPrimitive.reset(new MeshPrimitive(
			name.c_str(),
			indices,
			positions,
			normals,
			tangents,
			uv0,
			colors,
			joints,
			weights,
			meshParams));

		// This call will replace the newMeshPrimitive pointer if a duplicate MeshPrimitive already exists
		en::SceneManager::GetSceneData()->AddUniqueMeshPrimitive(newMeshPrimitive);

		return newMeshPrimitive;
	}


	MeshPrimitive::MeshPrimitive(
		char const* name,
		std::vector<uint32_t>* indices,
		std::vector<float>& positions,
		std::vector<float>* normals,
		std::vector<float>* tangents,
		std::vector<float>* uv0,
		std::vector<float>* colors,
		std::vector<uint8_t>* joints,
		std::vector<float>* weights,
		MeshPrimitiveParams const& meshParams)
		: NamedObject(name)
		, m_params(meshParams)
	{
		m_indexStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::Permanent,
			re::VertexStream::StreamType::Index,
			1, 
			re::VertexStream::DataType::UInt,
			re::VertexStream::Normalize::False,
			std::move(*indices)).get();

		m_vertexStreams[Slot::Position] = re::VertexStream::Create(
			re::VertexStream::Lifetime::Permanent,
			re::VertexStream::StreamType::Vertex,
			3,
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(positions)).get();

		if (normals && !normals->empty())
		{
			m_vertexStreams[Slot::Normal] = re::VertexStream::Create(
				re::VertexStream::Lifetime::Permanent,
				re::VertexStream::StreamType::Vertex,
				3,
				re::VertexStream::DataType::Float,
				re::VertexStream::Normalize::True,
				std::move(*normals)).get();
		}

		if (colors && !colors->empty())
		{
			m_vertexStreams[Slot::Color] = re::VertexStream::Create(
				re::VertexStream::Lifetime::Permanent,
				re::VertexStream::StreamType::Vertex,
				4,
				re::VertexStream::DataType::Float,
				re::VertexStream::Normalize::False,
				std::move(*colors)).get();
		}

		if (uv0 && !uv0->empty())
		{
			m_vertexStreams[Slot::UV0] = re::VertexStream::Create(
				re::VertexStream::Lifetime::Permanent,
				re::VertexStream::StreamType::Vertex,
				2,
				re::VertexStream::DataType::Float,
				re::VertexStream::Normalize::False,
				std::move(*uv0)).get();
		}

		if (tangents && !tangents->empty())
		{
			m_vertexStreams[Slot::Tangent] = re::VertexStream::Create(
				re::VertexStream::Lifetime::Permanent,
				re::VertexStream::StreamType::Vertex,
				4,
				re::VertexStream::DataType::Float,
				re::VertexStream::Normalize::True,
				std::move(*tangents)).get();
		}
		
		if (joints && !joints->empty())
		{
			m_vertexStreams[Slot::Joints] = re::VertexStream::Create(
				re::VertexStream::Lifetime::Permanent,
				re::VertexStream::StreamType::Vertex,
				1,
				re::VertexStream::DataType::UByte,
				re::VertexStream::Normalize::False,
				std::move(*joints)).get();
		}

		if (weights && !weights->empty())
		{
			m_vertexStreams[Slot::Weights] = re::VertexStream::Create(
				re::VertexStream::Lifetime::Permanent,
				re::VertexStream::StreamType::Vertex,
				1,
				re::VertexStream::DataType::Float,
				re::VertexStream::Normalize::False,
				std::move(*weights)).get();
		}

		ComputeDataHash();
	}


	void MeshPrimitive::Destroy()
	{
		for (uint8_t slotIdx = 0; slotIdx < Slot_Count; slotIdx++)
		{
			m_vertexStreams[slotIdx] = nullptr;
		}
	}


	void MeshPrimitive::ComputeDataHash()
	{
		AddDataBytesToHash(&m_params, sizeof(MeshPrimitiveParams));

		// Vertex data streams:
		for (size_t i = 0; i < m_vertexStreams.size(); i++)
		{
			if (m_vertexStreams[i])
			{
				AddDataBytesToHash(m_vertexStreams[i]->GetDataHash());
			}
		}
		if (m_indexStream)
		{
			AddDataBytesToHash(m_indexStream->GetDataHash());
		}
	}


	std::vector<re::VertexStream const*> MeshPrimitive::GetVertexStreams() const
	{
		std::vector<re::VertexStream const*> vertexStreamPtrs;
		vertexStreamPtrs.resize(Slot_Count, nullptr);

		for (uint8_t i = 0; i < Slot_Count; i++)
		{
			vertexStreamPtrs[i] = m_vertexStreams[i];
		}

		return vertexStreamPtrs;
	}


	char const* MeshPrimitive::SlotDebugNameToCStr(Slot slot)
	{
		switch (slot)
		{
		case Position: return ENUM_TO_STR(Position);
		case Normal: return ENUM_TO_STR(Normal);
		case Tangent: return ENUM_TO_STR(Tangent);
		case UV0: return ENUM_TO_STR(UV0);
		case Color: return ENUM_TO_STR(Color);
		case Joints: return ENUM_TO_STR(Joints);
		case Weights: return ENUM_TO_STR(Weights);
		default:
			SEAssertF("Invalid slot");
		}
		return "Invalid slot";
	}


	void MeshPrimitive::ShowImGuiWindow()
	{
		if (ImGui::CollapsingHeader(std::format("{}##{}", GetName(), GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Text(std::format("Draw mode: {}", DrawModeToCStr(m_params.m_topologyMode)).c_str());

			// ECS_CONVERSION TODO: Restore this functionality
			/*if (ImGui::CollapsingHeader(std::format("Material##{}", GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				m_meshMaterial->ShowImGuiWindow();
				ImGui::Unindent();
			}*/

			if (ImGui::CollapsingHeader(std::format("Vertex streams ({})##{}", m_vertexStreams.size(), GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				for (size_t i = 0; i < m_vertexStreams.size(); i++)
				{
					ImGui::Text(std::format("{}: {}", i, SlotToCStr(static_cast<gr::MeshPrimitive::Slot>(i))).c_str());
					if (m_vertexStreams[i])
					{
						m_vertexStreams[i]->ShowImGuiWindow();
					}
					else
					{
						ImGui::Text("<Empty>");
					}
					ImGui::Separator();
				}
				ImGui::Unindent();
			}

			// ECS_CONVERSION: TODO: Reimplement this functionality
			/*if (ImGui::CollapsingHeader(std::format("Local bounds##{}", GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				m_localBounds.ShowImGuiWindow();
				ImGui::Unindent();
			}*/
		}
	}
	
} // re


namespace meshfactory
{
	


	
} // meshfactory


