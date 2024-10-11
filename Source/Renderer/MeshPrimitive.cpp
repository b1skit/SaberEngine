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
				vertexStreams[i].m_typeIdx < vertexStreams[i + 1].m_typeIdx,
				"Vertex streams of the same type must be stored in monotoically-increasing source slot order");

			SEAssert(!seenSlots[static_cast<uint8_t>(vertexStreams[i].m_vertexStream->GetType())].contains(
				vertexStreams[i].m_typeIdx),
				"Duplicate slot index detected");

			// TODO: Re-enable this once we're no longer deferring vertex stream creation
			/*SEAssert(i + 1 == vertexStreams.size() || 
				vertexStreams[i].m_vertexStream->GetNumElements() == vertexStreams[i + 1].m_vertexStream->GetNumElements(),
				"Found vertex streams with mis-matching number of elements. This is unexpected");*/
			
			seenSlots[static_cast<uint8_t>(vertexStreams[i].m_vertexStream->GetType())].emplace(
				vertexStreams[i].m_typeIdx);
		}

#endif
	}


	inline void SortVertexStreams(std::vector<gr::MeshPrimitive::MeshVertexStream>& vertexStreams)
	{
		std::sort(vertexStreams.begin(), vertexStreams.end(), gr::MeshPrimitive::MeshVertexStreamComparator());
	}


	void GetNumMorphTargetBytes(
		std::vector<std::array<gr::VertexStream::CreateParams, gr::VertexStream::Type::Type_Count>> const& streams,
		size_t& numMorphTargetBytesOut,
		size_t& numMorphTargetsOut)
	{
		numMorphTargetBytesOut = 0;
		numMorphTargetsOut = 0;
		
		for (uint8_t typeIdx = 0; typeIdx < streams.size(); ++typeIdx)
		{
			for (uint8_t streamTypeIdx = 0; streamTypeIdx < gr::VertexStream::Type_Count; ++streamTypeIdx)
			{
				if (streamTypeIdx == gr::VertexStream::Index ||
					streams[typeIdx][streamTypeIdx].m_morphTargetData.empty())
				{
					continue;
				}
				for (auto const& morphData : streams[typeIdx][streamTypeIdx].m_morphTargetData)
				{
					numMorphTargetBytesOut += morphData.m_streamData->GetTotalNumBytes();
				}
				numMorphTargetsOut += streams[typeIdx][streamTypeIdx].m_morphTargetData.size();
			}
		}
	}


	// Process morph data: We interleave the target values to ensure displacements for each vertex are together
	// e.g. [t0, t1, t2, ...], for computing T = t + w[0] * t0 + w[1] * t1 + w[2] * t2
	bool InterleaveMorphData(
		std::vector<std::array<gr::VertexStream::CreateParams, gr::VertexStream::Type_Count>> const& streamCreateParams,
		std::vector<uint8_t>& interleavedMorphDataOut, // All morph data packed into a single, contiguous array
		std::vector<gr::MeshPrimitive::PackedMorphTargetMetadata>& interleavingMetadataOut)
	{
		bool foundMorphData = false;

		// Pre-parse the incoming stream data and count the number of morph targets
		size_t numMorphTargetBytes;
		size_t numMorphTargets;
		GetNumMorphTargetBytes(streamCreateParams, numMorphTargetBytes, numMorphTargets);

		if (numMorphTargets > 0)
		{
			foundMorphData = true;
			
			interleavingMetadataOut.clear();
			interleavingMetadataOut.reserve(numMorphTargets);

			interleavedMorphDataOut.clear();
			interleavedMorphDataOut.resize(numMorphTargetBytes);

			uint8_t* dst = interleavedMorphDataOut.data();
			size_t curByteOffset = 0;

			for (uint8_t typeIdx = 0; typeIdx < streamCreateParams.size(); ++typeIdx) // e.g. 0/1/2/3...
			{
				for (uint8_t streamTypeIdx = 0; streamTypeIdx < gr::VertexStream::Type_Count; ++streamTypeIdx) // e.g. Pos/Nml/Tan/UV
				{
					if (streamTypeIdx == gr::VertexStream::Index ||
						streamCreateParams[typeIdx][streamTypeIdx].m_morphTargetData.empty())
					{
						continue;
					}

					gr::VertexStream::CreateParams const& baseStreamParams = streamCreateParams[typeIdx][streamTypeIdx];

					const uint8_t numMorphTargets = util::CheckedCast<uint8_t>(baseStreamParams.m_morphTargetData.size());
					const size_t numMorphElements = baseStreamParams.m_morphTargetData[0].m_streamData->size();
					const uint8_t elementByteSize = baseStreamParams.m_morphTargetData[0].m_streamData->GetElementByteSize();

					SEAssert(elementByteSize % sizeof(float) == 0,
						"Unexpected element size. Currently assuming all morph data is Float/2/3/4");

					const uint32_t curFloatOffset = util::CheckedCast<uint32_t>(curByteOffset) / sizeof(float);
					const uint8_t vertexStrideFloats = numMorphTargets * elementByteSize / sizeof(float);
					const uint8_t elementStrideFloats = elementByteSize / sizeof(float);

					interleavingMetadataOut.emplace_back(gr::MeshPrimitive::PackedMorphTargetMetadata{
						.m_streamTypeIdx = streamTypeIdx,
						.m_typeIdx = typeIdx,
						.m_numMorphTargets = numMorphTargets,
						.m_firstFloatIdx = curFloatOffset,
						.m_vertexFloatStride = vertexStrideFloats,
						.m_elementFloatStride = elementStrideFloats,
						});

					// Interleave the morph target data, so each vertex's data is packed together:
					for (size_t elementIdx = 0; elementIdx < numMorphElements; ++elementIdx)
					{
						for (uint8_t morphTargetIdx = 0; morphTargetIdx < numMorphTargets; ++morphTargetIdx)
						{
							gr::VertexStream::MorphCreateParams const& morphCreateParams =
								baseStreamParams.m_morphTargetData[morphTargetIdx];

							SEAssert(numMorphElements == morphCreateParams.m_streamData->size() &&
								elementByteSize == morphCreateParams.m_streamData->GetElementByteSize(),
								"Unexpected element count or size");

							util::ByteVector const* morphData = morphCreateParams.m_streamData.get();

							memcpy(dst, morphData->GetElementPtr(elementIdx), elementByteSize);

							dst += elementByteSize;
							curByteOffset += elementByteSize;
						}
					}
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
		int8_t typeIdx /*= -1*/)
	{
		gr::VertexStream const* result = nullptr;

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
		
		// Each vector index streamCreateParams corresponds to the m_streamIdx of the entries in the array elements
		std::vector<MeshVertexStream> vertexStreams;
		vertexStreams.reserve(streamCreateParams.size() * gr::VertexStream::Type_Count); // + morph targets

		for (uint8_t typeIdx = 0; typeIdx < streamCreateParams.size(); ++typeIdx)
		{
			for (uint8_t streamTypeIdx = 0; streamTypeIdx < gr::VertexStream::Type_Count; ++streamTypeIdx)
			{
				if (streamTypeIdx == gr::VertexStream::Index)
				{
					continue; // Our single index stream is handled externally
				}

				if (streamCreateParams[typeIdx][streamTypeIdx].m_streamData)
				{
					vertexStreams.emplace_back(gr::MeshPrimitive::MeshVertexStream{
						.m_vertexStream = gr::VertexStream::Create(
							streamCreateParams[typeIdx][streamTypeIdx].m_streamDesc,
							std::move(*streamCreateParams[typeIdx][streamTypeIdx].m_streamData),
							queueBufferCreate).get(),
						.m_typeIdx = typeIdx,
						});
				}
			}
		}

		std::shared_ptr<gr::MeshPrimitive> newMeshPrim =
			gr::MeshPrimitive::Create(name, indexStream, std::move(vertexStreams), meshParams);


		// Pack morph data:
		std::vector<uint8_t> interleavedMorphData;
		std::vector<PackedMorphTargetMetadata> interleavedMorphMetadata;
		const bool hasMorphData = InterleaveMorphData(streamCreateParams, interleavedMorphData, interleavedMorphMetadata);

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

			newMeshPrim->m_interleavedMorphMetadata = std::move(interleavedMorphMetadata);

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


	gr::VertexStream const* MeshPrimitive::GetVertexStream(gr::VertexStream::Type streamType, uint8_t typeIdx) const
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