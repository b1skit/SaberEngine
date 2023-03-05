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

namespace re
{
	class MeshPrimitive final : public virtual en::NamedObject, public virtual en::HashedDataObject
	{
	public:
		struct PlatformParams : public IPlatformParams
		{
			virtual ~PlatformParams() = 0;
		};


	public:
		enum class DrawMode
		{
			Points,
			Lines,
			LineStrip,
			LineLoop,
			Triangles,
			TriangleStrip,
			TriangleFan,
			DrawMode_Count
		};

		struct MeshPrimitiveParams
		{
			DrawMode m_drawMode = DrawMode::Triangles;
		};

		// TODO: We'd prefer to have the Tangent and Bitanent/Binormal and reconstruct the Normal
		enum Slot // Binding index
		{
			Position	= 0, // vec3
			Normal		= 1, // vec3
			Tangent		= 2, // vec4
			UV0			= 3, // vec2
			Color		= 4, // vec4

			Joints		= 5, // tvec4<uint8_t>
			Weights		= 6, // vec4

			Indexes		= 7, // uint32_t Note: NOT a valid binding location

			Slot_Count,
			Slot_CountNoIndices = (Slot_Count - 1),
		};
		// Note: The order/indexing of this enum MUST match the vertex layout locations in SaberCommon.glsl, and be
		// correctly mapped in PipelineState_DX12.cpp
	
		static std::string GetSlotDebugName(Slot slot);


	public:
		MeshPrimitive(std::string const& name,
			std::vector<uint32_t>& indices,
			std::vector<float>& positions,
			glm::vec3 const& positionMinXYZ, // Pass gr::Bounds::k_invalidMinXYZ to compute bounds manually
			glm::vec3 const& positionMaxXYZ, // Pass gr::Bounds::k_invalidMaxXYZ to compute bounds manually
			std::vector<float>& normals,
			std::vector<float>& tangents,
			std::vector<float>& uv0,
			std::vector<float>& colors,	
			std::vector<uint8_t> joints,
			std::vector<float> weights,
			std::shared_ptr<gr::Material> material,
			re::MeshPrimitive::MeshPrimitiveParams const& meshParams);

		~MeshPrimitive(){ Destroy(); }
		
		
		// Getters/Setters:
		inline MeshPrimitiveParams const& GetMeshParams() const { return m_params; }

		inline std::shared_ptr<gr::Material> GetMeshMaterial() const { return m_meshMaterial; }

		inline gr::Bounds& GetBounds() { return m_localBounds; }
		inline gr::Bounds const& GetBounds() const { return m_localBounds; }
		void UpdateBounds(gr::Transform* transform); // TODO: Currently this assumes the MeshPrimitive is not skinned
		
		std::shared_ptr<re::VertexStream> GetVertexStream(Slot slot) const;

		inline PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		inline void SetPlatformParams(std::unique_ptr<re::MeshPrimitive::PlatformParams> params) { m_platformParams = std::move(params); }

	private:
		void Destroy();


	private:		
		MeshPrimitiveParams m_params;

		std::shared_ptr<gr::Material> m_meshMaterial;

		// API-specific mesh params:
		std::unique_ptr<PlatformParams> m_platformParams;

		std::vector<std::shared_ptr<re::VertexStream>> m_vertexStreams;

		gr::Bounds m_localBounds; // MeshPrimitive bounds, in local space		

		void ComputeDataHash() override;

	private:
		// No copying allowed
		MeshPrimitive() = delete;
		MeshPrimitive(MeshPrimitive const& rhs) = delete;
		MeshPrimitive(MeshPrimitive&& rhs) noexcept = delete;
		MeshPrimitive& operator=(MeshPrimitive const& rhs) = delete;
		MeshPrimitive& operator=(MeshPrimitive&& rhs) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline re::MeshPrimitive::PlatformParams::~PlatformParams() {};
} // re


namespace meshfactory
{
	extern std::shared_ptr<re::MeshPrimitive> CreateCube();

	enum class ZLocation
	{
		Near,
		Far
	};
	extern std::shared_ptr<re::MeshPrimitive> CreateFullscreenQuad(ZLocation zLocation);

	extern std::shared_ptr<re::MeshPrimitive> CreateQuad(
		glm::vec3 tl /*= vec3(-0.5f, 0.5f, 0.0f)*/,
		glm::vec3 tr /*= vec3(0.5f, 0.5f, 0.0f)*/,
		glm::vec3 bl /*= vec3(-0.5f, -0.5f, 0.0f)*/,
		glm::vec3 br /*= vec3(0.5f, -0.5f, 0.0f)*/);

	extern std::shared_ptr<re::MeshPrimitive> CreateSphere(
		float radius = 0.5f,
		size_t numLatSlices = 16,
		size_t numLongSlices = 16);

	// Creates a triangle in NDC
	extern std::shared_ptr<re::MeshPrimitive> CreateHelloTriangle(float scale = 1.f, float zDepth = 0.5f);
} // meshfactory