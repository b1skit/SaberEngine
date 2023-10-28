// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "IPlatformParams.h"
#include "Bounds.h"
#include "HashedDataObject.h"
#include "Material.h"
#include "NamedObject.h"
#include "VertexStream.h"


namespace gr
{
	class Transform;
}

namespace gr
{
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
			TriangleList,
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
		static std::shared_ptr<MeshPrimitive> Create(
			std::string const& name,
			std::vector<uint32_t>* indices,
			std::vector<float>& positions,
			glm::vec3 const& positionMinXYZ, // Pass gr::Bounds::k_invalidMinXYZ to compute bounds manually
			glm::vec3 const& positionMaxXYZ, // Pass gr::Bounds::k_invalidMaxXYZ to compute bounds manually
			std::vector<float>* normals,
			std::vector<float>* tangents,
			std::vector<float>* uv0,
			std::vector<float>* colors,
			std::vector<uint8_t>* joints,
			std::vector<float>* weights,
			std::shared_ptr<gr::Material> material,
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);

		~MeshPrimitive(){ Destroy(); }
		
		
		// Getters/Setters:
		MeshPrimitiveParams const& GetMeshParams() const;

		gr::Material* GetMeshMaterial() const;
		void SetMeshMaterial(std::shared_ptr<gr::Material> material);

		gr::Bounds& GetBounds();
		gr::Bounds const& GetBounds() const;
		void UpdateBounds(gr::Transform* transform); // TODO: Currently this assumes the MeshPrimitive is not skinned
		
		re::VertexStream const* GetIndexStream() const;
		re::VertexStream const* GetVertexStream(Slot slot) const;
		std::vector<re::VertexStream const*> GetVertexStreams() const;

		void ShowImGuiWindow();


	private:
		void Destroy();


	private:		
		MeshPrimitiveParams m_params;

		std::shared_ptr<gr::Material> m_meshMaterial;

		std::array<std::shared_ptr<re::VertexStream>, Slot_Count> m_vertexStreams;
		std::shared_ptr<re::VertexStream> m_indexStream;

		gr::Bounds m_localBounds; // MeshPrimitive bounds, in local space		

		void ComputeDataHash() override;


	private: // Private ctor: Use the Create factory instead
		MeshPrimitive(std::string const& name,
			std::vector<uint32_t>* indices,
			std::vector<float>& positions,
			glm::vec3 const& positionMinXYZ, // Pass gr::Bounds::k_invalidMinXYZ to compute bounds manually
			glm::vec3 const& positionMaxXYZ, // Pass gr::Bounds::k_invalidMaxXYZ to compute bounds manually
			std::vector<float>* normals,
			std::vector<float>* tangents,
			std::vector<float>* uv0,
			std::vector<float>* colors,
			std::vector<uint8_t>* joints,
			std::vector<float>* weights,
			std::shared_ptr<gr::Material> material,
			gr::MeshPrimitive::MeshPrimitiveParams const& meshParams);


	private:
		// No copying allowed
		MeshPrimitive() = delete;
		MeshPrimitive(MeshPrimitive const& rhs) = delete;
		MeshPrimitive(MeshPrimitive&& rhs) noexcept = delete;
		MeshPrimitive& operator=(MeshPrimitive const& rhs) = delete;
		MeshPrimitive& operator=(MeshPrimitive&& rhs) = delete;
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


	inline gr::Bounds& MeshPrimitive::GetBounds()
	{
		return m_localBounds;
	}


	inline gr::Bounds const& MeshPrimitive::GetBounds() const
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
} // re


namespace meshfactory
{
	extern std::shared_ptr<gr::MeshPrimitive> CreateCube();

	enum class ZLocation : uint8_t
	{
		Near,
		Far
	};
	extern std::shared_ptr<gr::MeshPrimitive> CreateFullscreenQuad(ZLocation zLocation);

	extern std::shared_ptr<gr::MeshPrimitive> CreateQuad(
		glm::vec3 tl /*= vec3(-0.5f, 0.5f, 0.0f)*/,
		glm::vec3 tr /*= vec3(0.5f, 0.5f, 0.0f)*/,
		glm::vec3 bl /*= vec3(-0.5f, -0.5f, 0.0f)*/,
		glm::vec3 br /*= vec3(0.5f, -0.5f, 0.0f)*/);

	extern std::shared_ptr<gr::MeshPrimitive> CreateSphere(
		float radius = 0.5f,
		size_t numLatSlices = 16,
		size_t numLongSlices = 16);

	// Creates a simple debug triangle.
	// Using the default arguments, the triangle will be in NDC.
	// Override the defaults to simulate a world-space transform (Reminder: We use a RHCS. Use negative zDepths to push
	// the triangle in front of the camera once a view-projection transformation is applied)
	extern std::shared_ptr<gr::MeshPrimitive> CreateHelloTriangle(float scale = 1.f, float zDepth = 0.5f);
} // meshfactory