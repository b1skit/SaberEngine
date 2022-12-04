#pragma once

#include <string>
#include <memory>

#include "Transform.h"
#include "Material.h"
#include "NamedObject.h"
#include "HashedDataObject.h"
#include "Bounds.h"


namespace re
{
	class MeshPrimitive : public virtual en::NamedObject, public virtual en::HashedDataObject
	{
	public:
		struct PlatformParams
		{
			virtual ~PlatformParams() = 0;

			bool m_isCreated = false;
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

	public:
		MeshPrimitive(std::string const& name,
			std::vector<uint32_t>& indices,
			std::vector<float>& positions,
			glm::vec3 const& positionMinXYZ, // Pass re::Bounds::k_invalidMinXYZ to compute bounds manually
			glm::vec3 const& positionMaxXYZ, // Pass re::Bounds::k_invalidMaxXYZ to compute bounds manually
			std::vector<float>& normals,
			std::vector<float>& tangents,
			std::vector<float>& uv0,
			std::vector<float>& colors,	
			std::vector<uint8_t> joints,
			std::vector<float> weights,
			std::shared_ptr<gr::Material> material,
			re::MeshPrimitive::MeshPrimitiveParams const& meshParams,
			gr::Transform* ownerTransform);

		~MeshPrimitive(){ Destroy(); }
		
		
		// Getters/Setters:
		inline MeshPrimitiveParams const& GetMeshParams() const { return m_params; }

		inline std::shared_ptr<gr::Material> MeshMaterial() { return m_meshMaterial; }

		inline gr::Transform*& GetOwnerTransform() { return m_ownerTransform; }
		inline gr::Transform const* GetOwnerTransform() const { return m_ownerTransform; }

		inline Bounds& GetBounds() { return m_localBounds; }
		inline Bounds const& GetBounds() const { return m_localBounds; }

		inline std::vector<float> const& GetPositions() const { return m_positions; }
		inline std::vector<float> const& GetNormals() const { return m_normals; }
		inline std::vector<float> const& GetColors() const { return m_colors; }
		inline std::vector<float> const& GetUV0() const { return m_uv0; }
		inline std::vector<float> const& GetTangents() const { return m_tangents; }
		inline std::vector<uint32_t> const& GetIndices() { return m_indices; }

		inline size_t NumIndices() const { return m_indices.size(); }

		inline std::unique_ptr<PlatformParams>& GetPlatformParams() { return m_platformParams; }
		inline std::unique_ptr<PlatformParams> const& GetPlatformParams() const { return m_platformParams; }


	private:
		void Destroy();


	private:		
		MeshPrimitiveParams m_params;

		std::shared_ptr<gr::Material> m_meshMaterial;

		// API-specific mesh params:
		std::unique_ptr<PlatformParams> m_platformParams;

		// Vertex data streams:
		std::vector<uint32_t> m_indices;

		std::vector<float> m_positions;		// vec3
		std::vector<float> m_normals;		// vec3
		std::vector<float> m_colors;		// vec4
		std::vector<float> m_uv0;			// vec2
		std::vector<float> m_tangents;		// vec4

		std::vector<uint8_t> m_joints;		// tvec4<uint8_t>
		std::vector<float> m_weights;		// vec4
		
		gr::Transform* m_ownerTransform;

		Bounds m_localBounds; // MeshPrimitive bounds, in local space		

		void ComputeDataHash() override;

	private:
		// Constructing a mesh primitive modifies the GPU state; disallow all copy/move semantics
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
} // meshfactory