// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "IPlatformParams.h"
#include "Bounds.h"
#include "HashedDataObject.h"
#include "Material.h"
#include "NamedObject.h"
#include "VertexStream.h"


namespace fr
{
	class GameplayManager;
	class Relationship;
}

namespace gr
{
	class Transform;


	class MeshPrimitive final : public virtual en::NamedObject, public virtual en::HashedDataObject
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
		struct MeshPrimitiveComponent
		{
			// MeshPrimitives are held in the SceneData so duplicate data can be shared
			gr::MeshPrimitive const* m_meshPrimitive;
		};


		struct RenderData
		{
			MeshPrimitiveParams m_meshPrimitiveParams;
			std::array<re::VertexStream const*, Slot_Count> m_vertexStreams;
			re::VertexStream const* m_indexStream;
			
			uint64_t m_dataHash;
		};
		static RenderData GetRenderData(MeshPrimitiveComponent const&);


	public:
		static entt::entity AttachMeshPrimitiveConcept(
			entt::entity meshConcept,
			char const* name,
			std::vector<uint32_t>* indices,
			std::vector<float>& positions,
			glm::vec3 const& positionMinXYZ, // Pass fr::BoundsConcept::k_invalidMinXYZ to compute bounds manually
			glm::vec3 const& positionMaxXYZ, // Pass fr::BoundsConcept::k_invalidMaxXYZ to compute bounds manually
			std::vector<float>* normals,
			std::vector<float>* tangents,
			std::vector<float>* uv0,
			std::vector<float>* colors,
			std::vector<uint8_t>* joints,
			std::vector<float>* weights,
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);


	public:
		[[nodiscard]] static std::shared_ptr<MeshPrimitive> Create(
			std::string const& name,
			std::vector<uint32_t>* indices,
			std::vector<float>& positions,
			glm::vec3 const& positionMinXYZ, // Pass fr::BoundsConcept::k_invalidMinXYZ to compute bounds manually
			glm::vec3 const& positionMaxXYZ, // Pass fr::BoundsConcept::k_invalidMaxXYZ to compute bounds manually
			std::vector<float>* normals,
			std::vector<float>* tangents,
			std::vector<float>* uv0,
			std::vector<float>* colors,
			std::vector<uint8_t>* joints,
			std::vector<float>* weights,
			std::shared_ptr<gr::Material> material,
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);

		MeshPrimitive(MeshPrimitive const& rhs) = default;
		MeshPrimitive& operator=(MeshPrimitive const& rhs) = default;
		MeshPrimitive(MeshPrimitive&& rhs) noexcept = default;
		MeshPrimitive& operator=(MeshPrimitive&& rhs) = default;

		~MeshPrimitive(){ Destroy(); }
		
		
		MeshPrimitiveParams const& GetMeshParams() const;

		gr::Material* GetMeshMaterial() const;
		void SetMeshMaterial(std::shared_ptr<gr::Material> material);

		fr::Bounds const& GetBounds() const;
		
		re::VertexStream const* GetIndexStream() const;
		re::VertexStream const* GetVertexStream(Slot slot) const;
		std::vector<re::VertexStream const*> GetVertexStreams() const;

		void ShowImGuiWindow();


	private:
		void Destroy();


	private:		
		MeshPrimitiveParams m_params;

		// ECS_CONVERSION: TODO: These should be raw pointers
		std::shared_ptr<gr::Material> m_meshMaterial; 

		std::array<std::shared_ptr<re::VertexStream>, Slot_Count> m_vertexStreams;
		std::shared_ptr<re::VertexStream> m_indexStream;

		fr::Bounds m_localBounds; // MeshPrimitive bounds, in local space		

		void ComputeDataHash() override;


	private: // Private ctor: Use the Create factory instead
		MeshPrimitive(char const* name,
			std::vector<uint32_t>* indices,
			std::vector<float>& positions,
			glm::vec3 const& positionMinXYZ, // Pass fr::BoundsConcept::k_invalidMinXYZ to compute bounds manually
			glm::vec3 const& positionMaxXYZ, // Pass fr::BoundsConcept::k_invalidMaxXYZ to compute bounds manually
			std::vector<float>* normals,
			std::vector<float>* tangents,
			std::vector<float>* uv0,
			std::vector<float>* colors,
			std::vector<uint8_t>* joints,
			std::vector<float>* weights,
			std::shared_ptr<gr::Material> material,
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);


	private: // No copying allowed
		MeshPrimitive() = delete;
	};


	inline MeshPrimitive::MeshPrimitiveParams const& MeshPrimitive::GetMeshParams() const
	{
		return m_params;
	}


	inline gr::Material* MeshPrimitive::GetMeshMaterial() const
	{
		return m_meshMaterial.get();
	}
	
	
	inline void MeshPrimitive::SetMeshMaterial(std::shared_ptr<gr::Material> material)
	{
		m_meshMaterial = material;
	};


	inline fr::Bounds const& MeshPrimitive::GetBounds() const
	{
		return m_localBounds;
	}


	inline re::VertexStream const* MeshPrimitive::GetIndexStream() const
	{
		return m_indexStream.get();
	}


	inline re::VertexStream const* MeshPrimitive::GetVertexStream(Slot slot) const
	{
		return m_vertexStreams[slot].get();
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline gr::MeshPrimitive::PlatformParams::~PlatformParams() {};
}