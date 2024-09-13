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


	void ValidateVertexStreams(std::vector<gr::MeshPrimitive::MeshVertexStream> const& vertexStreams)
	{
#if defined(_DEBUG)

		SEAssert(!vertexStreams.empty(), "Must have at least 1 vertex stream");

		std::array<std::unordered_set<uint8_t>, static_cast<uint8_t>(re::VertexStream::Type::Type_Count)> seenSlots;
		for (size_t i = 0; i < vertexStreams.size(); ++i)
		{
			SEAssert(vertexStreams[i].m_vertexStream != nullptr, "Found a null vertex stream in the input");

			SEAssert(i + 1 == vertexStreams.size() ||
				vertexStreams[i].m_vertexStream->GetType() != vertexStreams[i + 1].m_vertexStream->GetType() ||
				vertexStreams[i].m_typeIdx < vertexStreams[i + 1].m_typeIdx,
				"Vertex streams of the same type must be stored in monotoically-increasing source slot order");

			SEAssert(!seenSlots[static_cast<uint8_t>(vertexStreams[i].m_vertexStream->GetType())].contains(
				vertexStreams[i].m_typeIdx),
				"Duplicate slot index detected");
			
			seenSlots[static_cast<uint8_t>(vertexStreams[i].m_vertexStream->GetType())].emplace(
				vertexStreams[i].m_typeIdx);
		}

#endif
	}


	inline void SortVertexStreams(std::vector<gr::MeshPrimitive::MeshVertexStream>& vertexStreams)
	{
		std::sort(vertexStreams.begin(), vertexStreams.end(), gr::MeshPrimitive::MeshVertexStreamComparator());
	}
}

namespace gr
{
	re::VertexStream const* MeshPrimitive::RenderData::GetVertexStreamFromRenderData(
		gr::MeshPrimitive::RenderData const& meshPrimRenderData,
		re::VertexStream::Type streamType,
		int8_t typeIdx /*= -1*/)
	{
		re::VertexStream const* result = nullptr;

		for (uint8_t streamIdx = 0; streamIdx < meshPrimRenderData.m_vertexStreams.size(); ++streamIdx)
		{
			if (meshPrimRenderData.m_vertexStreams[streamIdx]->GetType() == streamType)
			{
				if (typeIdx < 0)
				{
					result = meshPrimRenderData.m_vertexStreams[streamIdx];
				}
				else 
				{
					const uint8_t offsetIdx = streamIdx + typeIdx;

					if (offsetIdx < meshPrimRenderData.m_vertexStreams.size() &&
						meshPrimRenderData.m_vertexStreams[offsetIdx]->GetType() == streamType)
					{
						result = meshPrimRenderData.m_vertexStreams[offsetIdx];
					}
				}
				break;
			}
		}

		return result;
	}


	std::shared_ptr<MeshPrimitive> MeshPrimitive::Create(
		std::string const& name,
		re::VertexStream const* indexStream,
		std::vector<MeshVertexStream>&& vertexStreams,
		gr::MeshPrimitive::MeshPrimitiveParams const& meshParams)
	{
		std::shared_ptr<MeshPrimitive> newMeshPrimitive;
		newMeshPrimitive.reset(new MeshPrimitive(
			name.c_str(),
			indexStream,
			std::move(vertexStreams),
			meshParams));

		// This call will replace the newMeshPrimitive pointer if a duplicate MeshPrimitive already exists
		re::RenderManager::GetSceneData()->AddUniqueMeshPrimitive(newMeshPrimitive);

		return newMeshPrimitive;
	}


	MeshPrimitive::MeshPrimitive(
		char const* name,
		re::VertexStream const* indexStream,
		std::vector<MeshVertexStream>&& vertexStreams,
		MeshPrimitiveParams const& meshParams)
		: INamedObject(name)
		, m_params(meshParams)
		, m_indexStream(indexStream)
		, m_vertexStreams(std::move(vertexStreams))
	{
		SortVertexStreams(m_vertexStreams);

		ValidateVertexStreams(m_vertexStreams); // _DEBUG only

		ComputeDataHash();
	}


	re::VertexStream const* MeshPrimitive::GetVertexStream(re::VertexStream::Type streamType, uint8_t typeIdx) const
	{
		auto result = std::lower_bound(
			m_vertexStreams.begin(),
			m_vertexStreams.end(),
			MeshPrimitive::MeshVertexStreamComparisonData{
				.m_streamType = streamType,
				.m_typeIdx = typeIdx },
				MeshPrimitive::MeshVertexStreamComparator());

		SEAssert(result != m_vertexStreams.end() &&
			result->m_vertexStream->GetType() == streamType &&
			result->m_typeIdx == typeIdx,
			"Failed to find a vertex stream of the given type and source type index. This is probably a surprise");

		return result->m_vertexStream;
	}


	void MeshPrimitive::ComputeDataHash()
	{
		AddDataBytesToHash(&m_params, sizeof(MeshPrimitiveParams));

		if (m_indexStream)
		{
			AddDataBytesToHash(m_indexStream->GetDataHash());
		}
		for (size_t i = 0; i < m_vertexStreams.size(); i++)
		{
			AddDataBytesToHash(m_vertexStreams[i].m_vertexStream->GetDataHash());

			for (size_t i = 0; i < m_vertexStreams[i].m_morphTargets.size(); i++)
			{
				AddDataBytesToHash(m_vertexStreams[i].m_morphTargets[i]->GetDataHash());
			}
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
					ImGui::Text(std::format("{}:", i).c_str());
					m_vertexStreams[i].m_vertexStream->ShowImGuiWindow();


					ImGui::BeginDisabled(m_vertexStreams[i].m_morphTargets.empty());
					if (ImGui::CollapsingHeader(
						std::format("Morph targets ({})##{}", m_vertexStreams[i].m_morphTargets.size(), GetUniqueID()).c_str(),
						ImGuiTreeNodeFlags_None))
					{
						ImGui::Indent();
						for (size_t i = 0; i < m_vertexStreams[i].m_morphTargets.size(); i++)
						{
							ImGui::Text(std::format("{}:", i).c_str());
							m_vertexStreams[i].m_morphTargets[i]->ShowImGuiWindow();
							ImGui::Separator();
						}
						ImGui::Unindent();
					}
					ImGui::EndDisabled();


					ImGui::Separator();
				}
				ImGui::Unindent();
			}

			ImGui::Unindent();
		}
	}	
}