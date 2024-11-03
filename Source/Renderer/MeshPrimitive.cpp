// © 2022 Adam Badke. All rights reserved.
#include "MeshPrimitive.h"
#include "RenderManager.h"
#include "SysInfo_Platform.h"


namespace
{
	constexpr char const* TopologyModeToCStr(gr::MeshPrimitive::PrimitiveTopology drawMode)
	{
		switch (drawMode)
		{
		case gr::MeshPrimitive::PrimitiveTopology::PointList: return "PointList";
		case gr::MeshPrimitive::PrimitiveTopology::LineList: return "LineList";
		case gr::MeshPrimitive::PrimitiveTopology::LineStrip: return "LineStrip";
		case gr::MeshPrimitive::PrimitiveTopology::TriangleList: return "TriangleList";
		case gr::MeshPrimitive::PrimitiveTopology::TriangleStrip: return "TriangleStrip";
		case gr::MeshPrimitive::PrimitiveTopology::LineListAdjacency: return "LineListAdjacency";
		case gr::MeshPrimitive::PrimitiveTopology::LineStripAdjacency: return "LineStripAdjacency";
		case gr::MeshPrimitive::PrimitiveTopology::TriangleListAdjacency: return "TriangleListAdjacency";
		case gr::MeshPrimitive::PrimitiveTopology::TriangleStripAdjacency: return "TriangleStripAdjacency";
		default: return "INVALID TOPOLOGY MODE";
		}
	}


	void ValidateVertexStreams(std::vector<gr::MeshPrimitive::MeshVertexStream> const& vertexStreams)
	{
#if defined(_DEBUG)

		SEAssert(!vertexStreams.empty(), "Must have at least 1 vertex stream");

		std::array<std::unordered_set<uint8_t>, static_cast<uint8_t>(gr::VertexStream::Type::Type_Count)> seenSlots;
		for (size_t i = 0; i < vertexStreams.size(); ++i)
		{
			SEAssert(vertexStreams[i].m_vertexStream != nullptr, "Found a null vertex stream in the input");

			SEAssert(i + 1 == vertexStreams.size() ||
				vertexStreams[i].m_vertexStream->GetType() != vertexStreams[i + 1].m_vertexStream->GetType() ||
				vertexStreams[i].m_setIdx < vertexStreams[i + 1].m_setIdx,
				"Vertex streams of the same type must be stored in monotoically-increasing source slot order");

			SEAssert(!seenSlots[static_cast<uint8_t>(vertexStreams[i].m_vertexStream->GetType())].contains(
				vertexStreams[i].m_setIdx),
				"Duplicate slot index detected");

			// TODO: Re-enable this once we're no longer deferring vertex stream creation
			/*SEAssert(i + 1 == vertexStreams.size() || 
				vertexStreams[i].m_vertexStream->GetNumElements() == vertexStreams[i + 1].m_vertexStream->GetNumElements(),
				"Found vertex streams with mis-matching number of elements. This is unexpected");*/
			
			seenSlots[static_cast<uint8_t>(vertexStreams[i].m_vertexStream->GetType())].emplace(
				vertexStreams[i].m_setIdx);
		}

#endif
	}


	inline void SortVertexStreams(std::vector<gr::MeshPrimitive::MeshVertexStream>& vertexStreams)
	{
		std::sort(vertexStreams.begin(), vertexStreams.end(), gr::MeshPrimitive::MeshVertexStreamComparator());
	}


	void GetNumMorphTargetBytes(
		std::vector<std::array<gr::VertexStream::CreateParams, gr::VertexStream::Type::Type_Count>> const& streams,
		size_t& totalNumMorphTargetBytesOut,
		size_t& totalNumMorphTargetsOut)
	{
		totalNumMorphTargetBytesOut = 0;
		totalNumMorphTargetsOut = 0;
		
		for (uint8_t setIdx = 0; setIdx < streams.size(); ++setIdx)
		{
			for (uint8_t streamTypeIdx = 0; streamTypeIdx < gr::VertexStream::Type_Count; ++streamTypeIdx)
			{
				if (streamTypeIdx == gr::VertexStream::Index ||
					streams[setIdx][streamTypeIdx].m_morphTargetData.empty())
				{
					continue;
				}
				for (auto const& morphData : streams[setIdx][streamTypeIdx].m_morphTargetData)
				{
					totalNumMorphTargetBytesOut += morphData.m_displacementData->GetTotalNumBytes();
				}
				totalNumMorphTargetsOut += streams[setIdx][streamTypeIdx].m_morphTargetData.size();
			}
		}
	}


	// Process morph data: We interleave the target values to ensure displacements for each vertex are together
	// e.g. [t0, t1, t2, ...], for computing T = t + w[0] * t0 + w[1] * t1 + w[2] * t2
	bool InterleaveMorphData(
		size_t totalVerts,
		std::vector<std::array<gr::VertexStream::CreateParams, gr::VertexStream::Type_Count>> const& streamCreateParams,
		std::vector<uint8_t>& interleavedDataOut, // All morph data packed into a single, contiguous array
		gr::MeshPrimitive::MorphTargetMetadata& interleavingMetadataOut)
	{
		bool foundMorphData = false;

		// Pre-parse the incoming stream data and count the number of morph targets
		size_t totalNumMorphTargetBytes;
		size_t totalNumMorphTargets;
		GetNumMorphTargetBytes(streamCreateParams, totalNumMorphTargetBytes, totalNumMorphTargets);

		if (totalNumMorphTargets > 0)
		{
			foundMorphData = true;
			
			interleavedDataOut.clear();
			interleavedDataOut.resize(totalNumMorphTargetBytes);

			interleavingMetadataOut.m_morphByteStride = 0; // We'll update this as we go

			uint8_t metadataIdx = 0;

			// First, parse the data to build our metadata. 
			// We iterate over our morph displacements in the same order they're packed on the MeshPrimitive
			for (uint8_t setIdx = 0; setIdx < streamCreateParams.size(); ++setIdx)
			{
				for (uint8_t streamTypeIdx = 0; streamTypeIdx < gr::VertexStream::Type_Count; ++streamTypeIdx)
				{
					if (streamTypeIdx == gr::VertexStream::Index ||
						streamCreateParams[setIdx][streamTypeIdx].m_morphTargetData.empty())
					{
						continue;
					}

					gr::VertexStream::CreateParams const& curStreamParams = streamCreateParams[setIdx][streamTypeIdx];

					gr::MeshPrimitive::PackingMetadata& metadata = interleavingMetadataOut.m_perStreamMetadata[metadataIdx];
					metadata = {0};

					// Interleave the data so all of the displacement targets for a vertex are packed together:
					// i.e. {v0.t0, v0.t1, v0.t2, v1.t0, v1.t1, v1.t2, ..., vn.t0, vn.t1, vn.t2}
					const size_t numDisplacements = curStreamParams.m_morphTargetData.size();

					if (interleavingMetadataOut.m_maxMorphTargets == 0)
					{
						interleavingMetadataOut.m_maxMorphTargets = util::CheckedCast<uint8_t>(numDisplacements);
					}
					SEAssert(numDisplacements == interleavingMetadataOut.m_maxMorphTargets,
						"All vertex attributes with morph targets must have the same number of them");

					for (size_t displacementIdx = 0; displacementIdx < numDisplacements; ++displacementIdx)
					{
						gr::VertexStream::MorphData const& curDisplacement = 
							curStreamParams.m_morphTargetData[displacementIdx];

						const uint8_t displacementByteSize = DataTypeToByteStride(curDisplacement.m_dataType);

						if (displacementIdx == 0)
						{
							metadata.m_firstByteOffset = interleavingMetadataOut.m_morphByteStride;
							metadata.m_byteStride = displacementByteSize;
							metadata.m_numComponents = DataTypeToNumComponents(curDisplacement.m_dataType);
						}

						interleavingMetadataOut.m_morphByteStride += displacementByteSize;

						SEAssert(curStreamParams.m_morphTargetData[0].m_displacementData->size() ==
							curDisplacement.m_displacementData->size() &&
							curStreamParams.m_morphTargetData[0].m_displacementData->GetElementByteSize() ==
							curDisplacement.m_displacementData->GetElementByteSize(),
							"Unexpected element count or size");
					}

					++metadataIdx;
				}
			}

			// Now that we know the layout, we can interleave the data:
			metadataIdx = 0;
			for (uint8_t setIdx = 0; setIdx < streamCreateParams.size(); ++setIdx)
			{
				for (uint8_t streamTypeIdx = 0; streamTypeIdx < gr::VertexStream::Type_Count; ++streamTypeIdx)
				{
					if (streamTypeIdx == gr::VertexStream::Index ||
						streamCreateParams[setIdx][streamTypeIdx].m_morphTargetData.empty())
					{
						continue;
					}

					gr::VertexStream::CreateParams const& curStreamParams = streamCreateParams[setIdx][streamTypeIdx];
					const size_t numDisplacements = curStreamParams.m_morphTargetData.size();

					gr::MeshPrimitive::PackingMetadata const& metadata =
						interleavingMetadataOut.m_perStreamMetadata[metadataIdx];

					for (size_t displacementIdx = 0; displacementIdx < numDisplacements; ++displacementIdx)
					{
						gr::VertexStream::MorphData const& curDisplacement =
							curStreamParams.m_morphTargetData[displacementIdx];

						util::ByteVector const* srcMorphData = curDisplacement.m_displacementData.get();

						SEAssert(srcMorphData->GetElementByteSize() == metadata.m_byteStride,
							"Stride does not match data byte size");

						uint8_t* dst = interleavedDataOut.data() + 
							metadata.m_firstByteOffset + (displacementIdx * metadata.m_byteStride);

						for (uint32_t vertIdx = 0; vertIdx < totalVerts; ++vertIdx)
						{
							memcpy(dst, srcMorphData->GetElementPtr(vertIdx), metadata.m_byteStride);
							dst += interleavingMetadataOut.m_morphByteStride;
						}
					}

					++metadataIdx;
				}
			}
		}

		return foundMorphData;
	}
}

namespace gr
{
	gr::VertexStream const* MeshPrimitive::RenderData::GetVertexStreamFromRenderData(
		gr::MeshPrimitive::RenderData const& meshPrimRenderData,
		gr::VertexStream::Type streamType,
		int8_t setIdx /*= -1*/)
	{
		gr::VertexStream const* result = nullptr;

		for (uint8_t streamIdx = 0; streamIdx < meshPrimRenderData.m_vertexStreams.size(); ++streamIdx)
		{
			if (meshPrimRenderData.m_vertexStreams[streamIdx]->GetType() == streamType)
			{
				if (setIdx < 0)
				{
					result = meshPrimRenderData.m_vertexStreams[streamIdx];
				}
				else 
				{
					const uint8_t offsetIdx = streamIdx + setIdx;

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
		gr::VertexStream const* indexStream,
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


	std::shared_ptr<MeshPrimitive> MeshPrimitive::Create(
		std::string const& name,
		std::vector<std::array<gr::VertexStream::CreateParams, gr::VertexStream::Type::Type_Count>>&& streamCreateParams,
		gr::MeshPrimitive::MeshPrimitiveParams const& meshParams,
		bool queueBufferCreate /*= true*/)
	{
		// NOTE: Currently we need to defer creating the VertexStream's backing re::Buffer from the front end thread 
		// with the queueBufferCreate ugliness here: If queueBufferCreate == true, the gr::VertexStream will enqueue a
		// render command to create the buffer on the render thread. This will go away once we have a proper async 
		// loading system

		SEAssert(streamCreateParams[0][gr::VertexStream::Index].m_streamData,
			"No index stream data. Indexes are required. We currently assume it will be in this fixed location");

		gr::VertexStream const* indexStream = 
			gr::VertexStream::Create(std::move(streamCreateParams[0][gr::VertexStream::Index]), queueBufferCreate).get();
		
		const size_t totalVerts = streamCreateParams[0][gr::VertexStream::Position].m_streamData->size();

		// Each vector index streamCreateParams corresponds to the m_setIdx of the entries in the array elements
		std::vector<MeshVertexStream> vertexStreams;
		vertexStreams.reserve(streamCreateParams.size() * gr::VertexStream::Type_Count); // + morph targets

		for (uint8_t setIdx = 0; setIdx < streamCreateParams.size(); ++setIdx)
		{
			for (uint8_t streamTypeIdx = 0; streamTypeIdx < gr::VertexStream::Type_Count; ++streamTypeIdx)
			{
				if (streamTypeIdx == gr::VertexStream::Index)
				{
					continue; // Our single index stream is handled externally
				}

				if (streamCreateParams[setIdx][streamTypeIdx].m_streamData)
				{
					SEAssert(streamCreateParams[setIdx][streamTypeIdx].m_streamData->size() == totalVerts,
						"Found a mismatched number of vertices between streams");

					vertexStreams.emplace_back(gr::MeshPrimitive::MeshVertexStream{
						.m_vertexStream = gr::VertexStream::Create(
							std::move(streamCreateParams[setIdx][streamTypeIdx]),
							queueBufferCreate).get(),
						.m_setIdx = setIdx,
						});
				}
			}
		}

		std::shared_ptr<gr::MeshPrimitive> newMeshPrim =
			gr::MeshPrimitive::Create(name, indexStream, std::move(vertexStreams), meshParams);


		// Pack morph data:
		std::vector<uint8_t> interleavedMorphData;
		const bool hasMorphData = InterleaveMorphData(
			totalVerts, streamCreateParams, interleavedMorphData, newMeshPrim->m_interleavedMorphMetadata);

		if (hasMorphData)
		{
			// TODO: This is illegal - if we call this from a front end thread, we'll be touching the buffer allocator
			// which is not allowed. Leaving this for now as it will go away once we have async loading, and we get away
			// with it if morph data is loaded before the 1st frame

			newMeshPrim->m_interleavedMorphData = re::Buffer::Create(
				std::format("{}_InterleavedMorphData", name),
				interleavedMorphData.data(),
				util::CheckedCast<uint32_t>(interleavedMorphData.size()),
				re::Buffer::BufferParams{
					.m_stagingPool = re::Buffer::StagingPool::Temporary,
					.m_memPoolPreference = re::Buffer::DefaultHeap,
					.m_accessMask = re::Buffer::GPURead,
					.m_usageMask = re::Buffer::Structured,
				});

			// Update the hash with the interleaved morph data
			newMeshPrim->AddDataBytesToHash(interleavedMorphData.data(),
				util::CheckedCast<uint32_t>(interleavedMorphData.size()));
		}

		return newMeshPrim;
	}


	MeshPrimitive::MeshPrimitive(
		char const* name,
		gr::VertexStream const* indexStream,
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


	gr::VertexStream const* MeshPrimitive::GetVertexStream(gr::VertexStream::Type streamType, uint8_t setIdx) const
	{
		auto result = std::lower_bound(
			m_vertexStreams.begin(),
			m_vertexStreams.end(),
			MeshPrimitive::MeshVertexStreamComparisonData{
				.m_streamType = streamType,
				.m_setIdx = setIdx },
				MeshPrimitive::MeshVertexStreamComparator());

		SEAssert(result != m_vertexStreams.end() &&
			result->m_vertexStream->GetType() == streamType &&
			result->m_setIdx == setIdx,
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
		}
	}


	void MeshPrimitive::ShowImGuiWindow() const
	{
		if (ImGui::CollapsingHeader(
			std::format("MeshPrimitive \"{}\"##{}", GetName(), GetUniqueID()).c_str(),ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			ImGui::Text(std::format("PrimitiveTopology: {}", TopologyModeToCStr(m_params.m_primitiveTopology)).c_str());

			if (ImGui::CollapsingHeader(
				std::format("Vertex streams ({})##{}", m_vertexStreams.size(), GetUniqueID()).c_str(), 
				ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				for (size_t i = 0; i < m_vertexStreams.size(); i++)
				{
					ImGui::Text(std::format("{}:", i).c_str());
					m_vertexStreams[i].m_vertexStream->ShowImGuiWindow();

					ImGui::Separator();
				}
				ImGui::Unindent();
			}

			ImGui::Unindent();
		}
	}	
}