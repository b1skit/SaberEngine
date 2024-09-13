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


	void ValidateVertexStreams(std::vector<re::VertexStream const*> const& vertexStreams, bool allowEmpty)
	{
#if defined(_DEBUG)

		SEAssert(!vertexStreams.empty() || allowEmpty, "Must have at least 1 vertex stream");

		std::array<std::unordered_set<uint8_t>, static_cast<uint8_t>(re::VertexStream::Type::Type_Count)> seenSlots;
		std::array<std::unordered_set<uint8_t>, static_cast<uint8_t>(re::VertexStream::Type::Type_Count)> seenMorphSlots;
		for (size_t i = 0; i < vertexStreams.size(); ++i)
		{
			SEAssert(vertexStreams[i] != nullptr, "Found a null vertex stream in the input");

			SEAssert(i + 1 == vertexStreams.size() ||
				vertexStreams[i]->GetType() != vertexStreams[i + 1]->GetType() ||
				vertexStreams[i]->GetSourceTypeIdx() < vertexStreams[i + 1]->GetSourceTypeIdx() ||
				(vertexStreams[i]->GetSourceTypeIdx() == vertexStreams[i + 1]->GetSourceTypeIdx() && 
					vertexStreams[i]->IsMorphData() && 
					vertexStreams[i + 1]->IsMorphData() &&
					vertexStreams[i]->GetMorphTargetIdx() < vertexStreams[i + 1]->GetMorphTargetIdx()),
				"Vertex streams of the same type must be stored in monotoically-increasing source slot order");

			SEAssert(!seenSlots[static_cast<uint8_t>(vertexStreams[i]->GetType())].contains(
					vertexStreams[i]->GetSourceTypeIdx()) ||
				(vertexStreams[i]->IsMorphData() && 
					!seenMorphSlots[static_cast<uint8_t>(vertexStreams[i]->GetType())].contains(
						vertexStreams[i]->GetMorphTargetIdx())),
				"Duplicate slot index detected");
			
			seenSlots[static_cast<uint8_t>(vertexStreams[i]->GetType())].emplace(
				vertexStreams[i]->GetSourceTypeIdx());

			if (vertexStreams[i]->IsMorphData())
			{
				seenMorphSlots[static_cast<uint8_t>(vertexStreams[i]->GetType())].emplace(
					vertexStreams[i]->GetMorphTargetIdx());
			}
		}

#endif
	}


	inline void SortVertexStreams(std::vector<re::VertexStream const*>& vertexStreams)
	{
		std::sort(vertexStreams.begin(), vertexStreams.end(), re::VertexStream::Comparator());
	}
}

namespace gr
{
	re::VertexStream const* MeshPrimitive::RenderData::GetVertexStreamFromRenderData(
		gr::MeshPrimitive::RenderData const& meshPrimRenderData,
		re::VertexStream::Type streamType,
		int8_t srcTypeIdx /*= -1*/)
	{
		re::VertexStream const* result = nullptr;

		for (auto const& stream : meshPrimRenderData.m_vertexStreams)
		{
			if (stream->GetType() == streamType &&
				(srcTypeIdx < 0 || stream->GetSourceTypeIdx() == srcTypeIdx))
			{
				result = stream;
				break;
			}
		}

		return result;
	}


	std::shared_ptr<MeshPrimitive> MeshPrimitive::Create(
		std::string const& name,
		re::VertexStream const* indexStream,
		std::vector<re::VertexStream const*>&& vertexStreams,
		gr::MeshPrimitive::MeshPrimitiveParams const& meshParams)
	{
		std::vector<re::VertexStream const*> emptyMorphTargets;
		return Create(name, indexStream, std::move(vertexStreams), std::move(emptyMorphTargets), meshParams);
	}


	std::shared_ptr<MeshPrimitive> MeshPrimitive::Create(
		std::string const& name,
		re::VertexStream const* indexStream,
		std::vector<re::VertexStream const*>&& vertexStreams,
		std::vector<re::VertexStream const*>&& morphTargets,
		gr::MeshPrimitive::MeshPrimitiveParams const& meshParams)
	{
		std::shared_ptr<MeshPrimitive> newMeshPrimitive;
		newMeshPrimitive.reset(new MeshPrimitive(
			name.c_str(),
			indexStream,
			std::move(vertexStreams),
			std::move(morphTargets),
			meshParams));

		// This call will replace the newMeshPrimitive pointer if a duplicate MeshPrimitive already exists
		re::RenderManager::GetSceneData()->AddUniqueMeshPrimitive(newMeshPrimitive);

		return newMeshPrimitive;
	}


	MeshPrimitive::MeshPrimitive(
		char const* name,
		re::VertexStream const* indexStream,
		std::vector<re::VertexStream const*>&& vertexStreams,
		std::vector<re::VertexStream const*>&& morphTargets,
		MeshPrimitiveParams const& meshParams)
		: INamedObject(name)
		, m_params(meshParams)
		, m_indexStream(indexStream)
		, m_vertexStreams(std::move(vertexStreams))
		, m_morphTargets(std::move(morphTargets))
	{
		SortVertexStreams(m_vertexStreams);
		SortVertexStreams(m_morphTargets);

		ValidateVertexStreams(m_vertexStreams, false); // _DEBUG only
		ValidateVertexStreams(m_morphTargets, true); // _DEBUG only

		ComputeDataHash();
	}


	re::VertexStream const* MeshPrimitive::GetVertexStream(re::VertexStream::Type streamType, uint8_t srcTypeIdx) const
	{
		auto result = std::lower_bound(
			m_vertexStreams.begin(),
			m_vertexStreams.end(),
			re::VertexStream::VertexComparisonData{
				.m_streamType = streamType,
				.m_typeIdx = srcTypeIdx },
			re::VertexStream::Comparator());

		SEAssert(*result != nullptr && 
			(*result)->GetType() == streamType && 
			(*result)->GetSourceTypeIdx() == srcTypeIdx,
			"Failed to find a vertex stream of the given type and source type index. This is probably a surprise");

		return *result;
	}


	re::VertexStream const* MeshPrimitive::GetMorphTargetStream(
		re::VertexStream::Type streamType, uint8_t srcTypeIdx, uint8_t morphTargetIdx) const
	{
		auto result = std::lower_bound(
			m_morphTargets.begin(),
			m_morphTargets.end(),
			re::VertexStream::MorphComparisonData{
				.m_streamType = streamType,
				.m_typeIdx = srcTypeIdx,
				.m_morphTargetIdx = morphTargetIdx },
			re::VertexStream::Comparator());

		SEAssert(*result != nullptr &&
			(*result)->GetType() == streamType &&
			(*result)->GetSourceTypeIdx() == srcTypeIdx &&
			(*result)->GetMorphTargetIdx() == morphTargetIdx,
			"Failed to find a vertex stream of the given type and source type index. This is probably a surprise");

		return *result;
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
			AddDataBytesToHash(m_vertexStreams[i]->GetDataHash());
		}
		for (size_t i = 0; i < m_morphTargets.size(); i++)
		{
			AddDataBytesToHash(m_morphTargets[i]->GetDataHash());
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
					m_vertexStreams[i]->ShowImGuiWindow();
					ImGui::Separator();
				}
				ImGui::Unindent();
			}

			ImGui::BeginDisabled(m_morphTargets.empty());
			if (ImGui::CollapsingHeader(
					std::format("Morph targets ({})##{}", m_morphTargets.size(), GetUniqueID()).c_str(),
					ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				for (size_t i = 0; i < m_morphTargets.size(); i++)
				{
					ImGui::Text(std::format("{}:", i).c_str());
					m_morphTargets[i]->ShowImGuiWindow();
					ImGui::Separator();
				}
				ImGui::Unindent();
			}
			ImGui::EndDisabled();

			ImGui::Unindent();
		}
	}	
}