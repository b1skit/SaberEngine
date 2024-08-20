// © 2022 Adam Badke. All rights reserved.
#include "MeshPrimitive.h"
#include "RenderManager.h"
#include "SysInfo_Platform.h"


namespace
{
	constexpr char const* TopologyModeToCStr(gr::MeshPrimitive::TopologyMode drawMode)
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
		default: return "INVALID TOPOLOGY MODE";
		}
	}


	void ValidateVertexStreams(std::vector<re::VertexStream const*> const& vertexStreams)
	{
#if defined(_DEBUG)

		SEAssert(!vertexStreams.empty(), "Must have at least 1 vertex stream");

		std::array<std::unordered_set<uint8_t>, static_cast<uint8_t>(re::VertexStream::Type::Type_Count)> seenSlots;
		for (size_t i = 0; i < vertexStreams.size(); ++i)
		{
			SEAssert(vertexStreams[i] != nullptr, "Found a null vertex stream in the input");

			SEAssert(i + 1 == vertexStreams.size() ||
				vertexStreams[i]->GetType() != vertexStreams[i + 1]->GetType() ||
				vertexStreams[i]->GetSourceSemanticIdx() + 1 == vertexStreams[i + 1]->GetSourceSemanticIdx(),
				"Vertex streams of the same type must be stored in monotoically-increasing source slot order");

			SEAssert(seenSlots[static_cast<uint8_t>(vertexStreams[i]->GetType())].contains(
					vertexStreams[i]->GetSourceSemanticIdx()) == false,
				"Duplicate slot index detected");
			
			seenSlots[static_cast<uint8_t>(vertexStreams[i]->GetType())].emplace(
				vertexStreams[i]->GetSourceSemanticIdx());
		}

#endif
	}


	inline void SortVertexStreams(std::vector<re::VertexStream const*>& vertexStreams)
	{
		std::sort(vertexStreams.begin(), vertexStreams.end(),
			[](re::VertexStream const* a, re::VertexStream const* b)
			{
				if (a->GetType() == b->GetType())
				{
					return a->GetSourceSemanticIdx() < b->GetSourceSemanticIdx();
				}
				return a->GetType() < b->GetType();
			});
	}
}

namespace gr
{
	re::VertexStream const* MeshPrimitive::RenderData::GetVertexStreamFromRenderData(
		gr::MeshPrimitive::RenderData const& meshPrimRenderData,
		re::VertexStream::Type streamType,
		int8_t semanticIdx /*= -1*/)
	{
		re::VertexStream const* result = nullptr;

		for (auto const& stream : meshPrimRenderData.m_vertexStreams)
		{
			if (stream->GetType() == streamType &&
				(semanticIdx < 0 || stream->GetSourceSemanticIdx() == semanticIdx))
			{
				result = stream;
				break;
			}
		}

		return result;
	}


	std::shared_ptr<MeshPrimitive> MeshPrimitive::Create(
		std::string const& name,
		std::vector<re::VertexStream const*>&& vertexStreams,
		re::VertexStream const* indexStream,
		gr::MeshPrimitive::MeshPrimitiveParams const& meshParams)
	{
		std::shared_ptr<MeshPrimitive> newMeshPrimitive;
		newMeshPrimitive.reset(new MeshPrimitive(
			name.c_str(),
			std::move(vertexStreams),
			indexStream,
			meshParams));

		// This call will replace the newMeshPrimitive pointer if a duplicate MeshPrimitive already exists
		re::RenderManager::GetSceneData()->AddUniqueMeshPrimitive(newMeshPrimitive);

		return newMeshPrimitive;
	}


	MeshPrimitive::MeshPrimitive(
		char const* name,
		std::vector<re::VertexStream const*>&& vertexStreams,
		re::VertexStream const* indexStream,
		MeshPrimitiveParams const& meshParams)
		: INamedObject(name)
		, m_params(meshParams)
		, m_indexStream(indexStream)
	{
		m_vertexStreams = std::move(vertexStreams);
		SortVertexStreams(m_vertexStreams);

		ValidateVertexStreams(m_vertexStreams); // _DEBUG only

		ComputeDataHash();
	}


	re::VertexStream const* MeshPrimitive::GetVertexStream(re::VertexStream::Type streamType, uint8_t semanticIdx) const
	{
		re::VertexStream const* result = nullptr;
		for (size_t streamIdx = 0; streamIdx < m_vertexStreams.size(); ++streamIdx)
		{
			if (m_vertexStreams[streamIdx]->GetType() == streamType &&
				m_vertexStreams[streamIdx]->GetSourceSemanticIdx() == semanticIdx)
			{
				result = m_vertexStreams[streamIdx];
				break;
			}
		}

		SEAssert(result != nullptr,
			"Failed to find a vertex stream of the given type and sematic index. This is probably a surprise");

		return result;
	}


	void MeshPrimitive::ComputeDataHash()
	{
		AddDataBytesToHash(&m_params, sizeof(MeshPrimitiveParams));

		// Vertex data streams:
		for (size_t i = 0; i < m_vertexStreams.size(); i++)
		{
			AddDataBytesToHash(m_vertexStreams[i]->GetDataHash());
		}
		if (m_indexStream)
		{
			AddDataBytesToHash(m_indexStream->GetDataHash());
		}
	}


	void MeshPrimitive::ShowImGuiWindow() const
	{
		if (ImGui::CollapsingHeader(
			std::format("MeshPrimitive \"{}\"##{}", GetName(), GetUniqueID()).c_str(),ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			ImGui::Text(std::format("Draw mode: {}", TopologyModeToCStr(m_params.m_topologyMode)).c_str());

			if (ImGui::CollapsingHeader(
				std::format("Vertex streams ({})##{}", m_vertexStreams.size(), GetUniqueID()).c_str(), 
				ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				for (size_t i = 0; i < m_vertexStreams.size(); i++)
				{
					ImGui::Text(std::format("{}: {}",
						i, 
						re::VertexStream::DataTypeToCStr(m_vertexStreams[i]->GetDataType())).c_str());
					m_vertexStreams[i]->ShowImGuiWindow();
					ImGui::Separator();
				}
				ImGui::Unindent();
			}

			ImGui::Unindent();
		}
	}	
}