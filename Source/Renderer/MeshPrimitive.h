// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Material.h"
#include "RenderObjectIDs.h"
#include "VertexStream.h"

#include "Core/Interfaces/IPlatformParams.h"
#include "Core/Interfaces/IHashedDataObject.h"
#include "Core/Interfaces/INamedObject.h"

#include "Shaders/Common/AnimationParams.h"


namespace gr
{
	class MeshPrimitive final : 
		public virtual core::INamedObject, 
		public virtual core::IHashedDataObject, 
		public virtual core::IUniqueID
	{
	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual ~PlatformParams() = default;
		};


	public:
		// Specific format the IA will use to interpret the topology contained within the vertex/index buffers.
		// Elements of the same basic type here can be used interchangeably with PSO's that map to the more general 
		// re::PipelineState::PrimitiveTopologyType. E.g. PrimitiveTopology::Line* -> PrimitiveTopologyType::Line
		enum class PrimitiveTopology : uint8_t
		{
			PointList,
			LineList,
			LineStrip,
			TriangleList, // Default
			TriangleStrip,
			LineListAdjacency,
			LineStripAdjacency,
			TriangleListAdjacency,
			TriangleStripAdjacency
		};

		struct MeshPrimitiveParams
		{
			PrimitiveTopology m_primitiveTopology = PrimitiveTopology::TriangleList;
		};

		struct MeshVertexStream
		{
			gr::VertexStream const* m_vertexStream = nullptr;
			uint8_t m_typeIdx = 0; // Index of m_vertexStream, w.r.t other streams of the same type. Used for sorting
		};

		struct MeshVertexStreamComparisonData
		{
			gr::VertexStream::Type m_streamType;
			uint8_t m_typeIdx;
		};
		struct MeshVertexStreamComparator
		{
			bool operator()(MeshVertexStream const&, MeshVertexStream const&);
			bool operator()(MeshVertexStream const&, MeshVertexStreamComparisonData const&);
		};


	public:
		struct PackingMetadata
		{
			uint8_t m_firstByteOffset;	// No. bytes from the start of the packing to 1st byte of this displacement
			uint8_t m_byteStride;		// No. bytes in 1 displacement (e.g. float3 = 12)
			uint8_t m_numComponents;	// No. components in 1 displacement (e.g. float3 = 3)
		};

		struct MorphTargetMetadata
		{
			uint8_t m_maxMorphTargets;	// A vertex may either have 0 or m_maxMorphTargets, exactly
			uint32_t m_morphByteStride;	// Total bytes for all interleaved displacements for 1 vertex (e.g. from Vn to Vn+1)

			std::array<PackingMetadata, gr::VertexStream::k_maxVertexStreams> m_perStreamMetadata;
		};

	public:
		struct RenderData
		{
			MeshPrimitiveParams m_meshPrimitiveParams;

			std::array<gr::VertexStream const*, gr::VertexStream::k_maxVertexStreams> m_vertexStreams;
			uint8_t m_numVertexStreams;

			gr::VertexStream const* m_indexStream;

			bool m_hasMorphTargets;
			std::shared_ptr<re::Buffer> m_interleavedMorphData;
			MorphTargetMetadata m_morphTargetMetadata;

			uint64_t m_dataHash;

			gr::RenderDataID m_owningMeshRenderDataID; // To access owning MeshRenderData


			// Helper: Get a specific vertex stream packed into a MeshPrimitive::RenderData.
			// If the typeIdx index < 0, the first matching type is returned
			static gr::VertexStream const* GetVertexStreamFromRenderData(
				gr::MeshPrimitive::RenderData const&, gr::VertexStream::Type, int8_t typeIdx = -1);
		};


		struct MeshRenderData
		{
			std::vector<float> m_morphTargetWeights;
		};


	public:
		[[nodiscard]] static std::shared_ptr<MeshPrimitive> Create(
			std::string const& name,
			gr::VertexStream const* indexStream,
			std::vector<MeshVertexStream>&& vertexStreams,
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);

		[[nodiscard]] static std::shared_ptr<MeshPrimitive> Create(
			std::string const& name,
			std::vector<std::array<gr::VertexStream::CreateParams, gr::VertexStream::Type::Type_Count>>&&,
			gr::MeshPrimitive::MeshPrimitiveParams const&,
			bool queueBufferCreate = true);

		MeshPrimitive(MeshPrimitive&& rhs) noexcept = default;
		MeshPrimitive& operator=(MeshPrimitive&& rhs) noexcept = default;
		~MeshPrimitive() = default;
		
		MeshPrimitiveParams const& GetMeshParams() const;

		gr::VertexStream const* GetIndexStream() const;
		
		gr::VertexStream const* GetVertexStream(gr::VertexStream::Type, uint8_t srcTypeIdx) const;
		std::vector<MeshVertexStream> const& GetVertexStreams() const;

		bool HasMorphTargets() const;
		std::shared_ptr<re::Buffer> GetInterleavedMorphDataBuffer() const;
		MorphTargetMetadata const& GetMorphTargetMetadata() const;

		void ShowImGuiWindow() const;


	private:		
		MeshPrimitiveParams m_params;

		gr::VertexStream const* m_indexStream;
		std::vector<MeshVertexStream> m_vertexStreams;	

		std::shared_ptr<re::Buffer> m_interleavedMorphData;
		MorphTargetMetadata m_interleavedMorphMetadata;


		void ComputeDataHash() override;


	private: // Private ctor: Use the Create factory instead
		MeshPrimitive(char const* name,
			gr::VertexStream const* indexStream,
			std::vector<MeshVertexStream>&& vertexStreams,
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);


	private: // No copying allowed
		MeshPrimitive() = delete;
		MeshPrimitive(MeshPrimitive const& rhs) = delete;
		MeshPrimitive& operator=(MeshPrimitive const& rhs) = delete;
	};


	inline MeshPrimitive::MeshPrimitiveParams const& MeshPrimitive::GetMeshParams() const
	{
		return m_params;
	}


	inline gr::VertexStream const* MeshPrimitive::GetIndexStream() const
	{
		return m_indexStream;
	}


	inline std::vector<MeshPrimitive::MeshVertexStream> const& MeshPrimitive::GetVertexStreams() const
	{
		return m_vertexStreams;
	}


	inline bool MeshPrimitive::HasMorphTargets() const
	{
		return m_interleavedMorphData != nullptr;
	}


	inline std::shared_ptr<re::Buffer> MeshPrimitive::GetInterleavedMorphDataBuffer() const
	{
		return m_interleavedMorphData;
	}


	inline MeshPrimitive::MorphTargetMetadata const& MeshPrimitive::GetMorphTargetMetadata() const
	{
		return m_interleavedMorphMetadata;
	}


	inline bool MeshPrimitive::MeshVertexStreamComparator::operator()(
		gr::MeshPrimitive::MeshVertexStream const& a, gr::MeshPrimitive::MeshVertexStream const& b)
	{
		if (a.m_vertexStream->GetType() == b.m_vertexStream->GetType())
		{
			return a.m_typeIdx < b.m_typeIdx;
		}
		return a.m_vertexStream->GetType() < b.m_vertexStream->GetType();
	}


	inline bool MeshPrimitive::MeshVertexStreamComparator::operator()(
		MeshPrimitive::MeshVertexStream const& a, MeshPrimitive::MeshVertexStreamComparisonData const& b)
	{
		if (a.m_vertexStream->GetType() == b.m_streamType)
		{
			return a.m_typeIdx < b.m_typeIdx;
		}
		return a.m_vertexStream->GetType() < b.m_streamType;
	}
}