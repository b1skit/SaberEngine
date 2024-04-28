// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "IPlatformParams.h"
#include "Core\Interfaces\IHashedDataObject.h"
#include "Material.h"
#include "Core\Interfaces\INamedObject.h"
#include "VertexStream.h"


namespace gr
{
	class MeshPrimitive final : public virtual core::INamedObject, public virtual core::IHashedDataObject
	{
	public:
		struct PlatformParams : public re::IPlatformParams
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

		// TODO: We'd prefer to have the Tangent and Bitangent/Binormal and reconstruct the Normal
		enum Slot : uint8_t // Binding index
		{
			Position	= 0, // vec3
			Normal		= 1, // vec3
			Tangent		= 2, // vec4
			UV0			= 3, // vec2
			// TODO: Support UV1
			Color		= 4, // vec4

			Joints		= 5, // tvec4<uint8_t>
			Weights		= 6, // vec4

			Slot_Count,
		};
		static_assert(Slot::Position == 0); // Position MUST be first
		// Note: The order/indexing of this enum MUST match the vertex layout locations in SaberCommon.glsl, and be
		// correctly mapped in PipelineState_DX12.cpp
		
	
		static char const* SlotDebugNameToCStr(Slot slot);

	public:
		struct RenderData
		{
			MeshPrimitiveParams m_meshPrimitiveParams;
			std::array<re::VertexStream const*, Slot_Count> m_vertexStreams;
			re::VertexStream const* m_indexStream;
			
			uint64_t m_dataHash;
		};


	public:
		[[nodiscard]] static std::shared_ptr<MeshPrimitive> Create(
			std::string const& name,
			std::vector<uint32_t>* indices,
			std::vector<float>& positions,
			std::vector<float>* normals,
			std::vector<float>* tangents,
			std::vector<float>* uv0,
			std::vector<float>* colors,
			std::vector<uint8_t>* joints,
			std::vector<float>* weights,
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);

		MeshPrimitive(MeshPrimitive&& rhs) noexcept = default;
		MeshPrimitive& operator=(MeshPrimitive&& rhs) = default;
		~MeshPrimitive() = default;
		
		MeshPrimitiveParams const& GetMeshParams() const;
	
		re::VertexStream const* GetIndexStream() const;
		re::VertexStream const* GetVertexStream(Slot slot) const;
		std::vector<re::VertexStream const*> GetVertexStreams() const;

		void ShowImGuiWindow() const;


	private:		
		MeshPrimitiveParams m_params;

		std::array<re::VertexStream const*, Slot_Count> m_vertexStreams;
		re::VertexStream const* m_indexStream;


		void ComputeDataHash() override;


	private: // Private ctor: Use the Create factory instead
		MeshPrimitive(char const* name,
			std::vector<uint32_t>* indices,
			std::vector<float>& positions,
			std::vector<float>* normals,
			std::vector<float>* tangents,
			std::vector<float>* uv0,
			std::vector<float>* colors,
			std::vector<uint8_t>* joints,
			std::vector<float>* weights,
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


	inline re::VertexStream const* MeshPrimitive::GetVertexStream(Slot slot) const
	{
		return m_vertexStreams[slot];
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline gr::MeshPrimitive::PlatformParams::~PlatformParams() {};
}