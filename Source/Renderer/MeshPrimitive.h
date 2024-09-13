// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Material.h"
#include "VertexStream.h"

#include "Core/Interfaces/IPlatformParams.h"
#include "Core/Interfaces/IHashedDataObject.h"
#include "Core/Interfaces/INamedObject.h"


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
		enum class TopologyMode : uint8_t
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
			TopologyMode m_topologyMode = TopologyMode::TriangleList;
		};

		struct MeshVertexStream
		{
			re::VertexStream const* m_vertexStream = nullptr;
			uint8_t m_typeIdx = 0; // Index of m_vertexStream, w.r.t other streams of the same type. Used for sorting

			std::vector<re::VertexStream const*> m_morphTargets;
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
		struct RenderData
		{
			MeshPrimitiveParams m_meshPrimitiveParams;

			std::array<re::VertexStream const*, re::VertexStream::k_maxVertexStreams> m_vertexStreams;
			uint8_t m_numVertexStreams;

			re::VertexStream const* m_indexStream;
			
			uint64_t m_dataHash;


			// Helper: Get a specific vertex stream packed into a MeshPrimitive::RenderData.
			// If the srcTypeIdx index < 0, the first matching type is returned
			static re::VertexStream const* GetVertexStreamFromRenderData(
				gr::MeshPrimitive::RenderData const&, re::VertexStream::Type, int8_t srcTypeIdx = -1);
		};	


	public:
		[[nodiscard]] static std::shared_ptr<MeshPrimitive> Create(
			std::string const& name,
			re::VertexStream const* indexStream,
			std::vector<MeshVertexStream>&& vertexStreams,
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);

		MeshPrimitive(MeshPrimitive&& rhs) noexcept = default;
		MeshPrimitive& operator=(MeshPrimitive&& rhs) = default;
		~MeshPrimitive() = default;
		
		MeshPrimitiveParams const& GetMeshParams() const;

		re::VertexStream const* GetIndexStream() const;
		
		re::VertexStream const* GetVertexStream(re::VertexStream::Type, uint8_t srcTypeIdx) const;
		std::vector<MeshVertexStream> const& GetVertexStreams() const;

		void ShowImGuiWindow() const;


	private:		
		MeshPrimitiveParams m_params;

		re::VertexStream const* m_indexStream;
		std::vector<MeshVertexStream> m_vertexStreams;


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