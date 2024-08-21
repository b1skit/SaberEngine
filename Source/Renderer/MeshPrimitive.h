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
			std::vector<re::VertexStream const*>&& vertexStreams,			
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);

		[[nodiscard]] static std::shared_ptr<MeshPrimitive> Create(
			std::string const& name,
			re::VertexStream const* indexStream,
			std::vector<re::VertexStream const*>&& vertexStreams,
			std::vector<re::VertexStream const*>&& morphTargets,
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);

		MeshPrimitive(MeshPrimitive&& rhs) noexcept = default;
		MeshPrimitive& operator=(MeshPrimitive&& rhs) = default;
		~MeshPrimitive() = default;
		
		MeshPrimitiveParams const& GetMeshParams() const;

		re::VertexStream const* GetIndexStream() const;
		
		re::VertexStream const* GetVertexStream(re::VertexStream::Type, uint8_t srcTypeIdx) const;
		std::vector<re::VertexStream const*> const& GetVertexStreams() const;

		re::VertexStream const* GetMorphTargetStream(
			re::VertexStream::Type, uint8_t srcTypeIdx, uint8_t morphTargetIdx) const;
		std::vector<re::VertexStream const*> const& GetMorphTargetStreams() const;

		void ShowImGuiWindow() const;


	private:		
		MeshPrimitiveParams m_params;

		re::VertexStream const* m_indexStream;
		std::vector<re::VertexStream const*> m_vertexStreams;
		std::vector<re::VertexStream const*> m_morphTargets;


		void ComputeDataHash() override;


	private: // Private ctor: Use the Create factory instead
		MeshPrimitive(char const* name,
			re::VertexStream const* indexStream,
			std::vector<re::VertexStream const*>&& vertexStreams,
			std::vector<re::VertexStream const*>&& morphTargets,
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


	inline std::vector<re::VertexStream const*> const& MeshPrimitive::GetVertexStreams() const
	{
		return m_vertexStreams;
	}


	inline std::vector<re::VertexStream const*> const& MeshPrimitive::GetMorphTargetStreams() const
	{
		return m_morphTargets;
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline gr::MeshPrimitive::PlatformParams::~PlatformParams() {};
}