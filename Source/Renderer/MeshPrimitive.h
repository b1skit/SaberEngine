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
	class MeshPrimitive final : public virtual core::INamedObject, public virtual core::IHashedDataObject
	{
	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual ~PlatformParams() = 0;
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
			re::VertexStream const* m_vertexStream = nullptr;
			uint8_t m_typeIdx = 0; // Index of m_vertexStream, w.r.t other streams of the same type. Used for sorting
		};

		struct MeshVertexStreamComparisonData
		{
			re::VertexStream::Type m_streamType;
			uint8_t m_typeIdx;
		};
		struct MeshVertexStreamComparator
		{
			bool operator()(MeshVertexStream const&, MeshVertexStream const&);
			bool operator()(MeshVertexStream const&, MeshVertexStreamComparisonData const&);
		};


	public:
		struct PackedMorphTargetMetadata
		{
			uint8_t m_streamTypeIdx;	// e.g. Pos/Nml/Tan/UV
			uint8_t m_typeIdx;			// e.g.0/1/2/3...

			uint8_t m_numMorphTargets;		// How many morph targets per vertex?
			uint32_t m_firstFloatIdx;		// Index of first float of a vertex's set of interleaved morph target elements
			uint8_t m_vertexFloatStride;	// Stride of all elements (# floats) for a whole set of morph target displacements
			uint8_t m_elementFloatStride;	// Stride of a single element (# floats), within a single morph target displacement			
		};


	public:
		struct RenderData
		{
			MeshPrimitiveParams m_meshPrimitiveParams;

			std::array<re::VertexStream const*, re::VertexStream::k_maxVertexStreams> m_vertexStreams;
			uint8_t m_numVertexStreams;

			re::VertexStream const* m_indexStream;

			bool m_hasMorphTargets;
			std::shared_ptr<re::Buffer> m_interleavedMorphData;
			
			uint64_t m_dataHash;

			gr::RenderDataID m_owningMeshRenderDataID; // To access owning MeshRenderData


			// Helper: Get a specific vertex stream packed into a MeshPrimitive::RenderData.
			// If the typeIdx index < 0, the first matching type is returned
			static re::VertexStream const* GetVertexStreamFromRenderData(
				gr::MeshPrimitive::RenderData const&, re::VertexStream::Type, int8_t typeIdx = -1);
		};


		struct MeshRenderData
		{
			std::array<float, AnimationData::k_numMorphTargets> m_morphWeights;
		};


	public:
		[[nodiscard]] static std::shared_ptr<MeshPrimitive> Create(
			std::string const& name,
			re::VertexStream const* indexStream,
			std::vector<MeshVertexStream>&& vertexStreams,
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);

		[[nodiscard]] static std::shared_ptr<MeshPrimitive> Create(
			std::string const& name,
			std::vector<std::array<re::VertexStream::CreateParams, re::VertexStream::Type::Type_Count>>&&,
			gr::MeshPrimitive::MeshPrimitiveParams const&,
			bool queueBufferCreate = true);

		MeshPrimitive(MeshPrimitive&& rhs) noexcept = default;
		MeshPrimitive& operator=(MeshPrimitive&& rhs) noexcept = default;
		~MeshPrimitive() = default;
		
		MeshPrimitiveParams const& GetMeshParams() const;

		re::VertexStream const* GetIndexStream() const;
		
		re::VertexStream const* GetVertexStream(re::VertexStream::Type, uint8_t srcTypeIdx) const;
		std::vector<MeshVertexStream> const& GetVertexStreams() const;

		bool HasMorphTargets() const;
		std::shared_ptr<re::Buffer> GetInterleavedMorphDataBuffer() const;

		void ShowImGuiWindow() const;


	private:		
		MeshPrimitiveParams m_params;

		re::VertexStream const* m_indexStream;
		std::vector<MeshVertexStream> m_vertexStreams;	

		std::shared_ptr<re::Buffer> m_interleavedMorphData;
		std::vector<PackedMorphTargetMetadata> m_interleavedMorphMetadata;


		void ComputeDataHash() override;


	private: // Private ctor: Use the Create factory instead
		MeshPrimitive(char const* name,
			re::VertexStream const* indexStream,
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


	inline re::VertexStream const* MeshPrimitive::GetIndexStream() const
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


	// We need to provide a destructor implementation since it's pure virtual
	inline gr::MeshPrimitive::PlatformParams::~PlatformParams() {};
}