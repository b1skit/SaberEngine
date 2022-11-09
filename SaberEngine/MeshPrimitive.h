#pragma once

#include <string>
#include <memory>

#include "Transform.h"
#include "Material.h"
#include "MeshPrimitive_Platform.h"
#include "NamedObject.h"
#include "HashedDataObject.h"
#include "Bounds.h"


namespace re
{
	class MeshPrimitive : public virtual en::NamedObject, public virtual en::HashedDataObject
	{
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
			std::vector<float>& positions,
			std::vector<float>& normals,
			std::vector<float>& colors,
			std::vector<float>& uv0,
			std::vector<float>& tangents,
			std::vector<uint32_t>& indices,
			std::shared_ptr<gr::Material> material,
			re::MeshPrimitive::MeshPrimitiveParams const& meshParams,
			gr::Transform* ownerTransform);		
		// TODO: Rearrange these args to match shader vertex attribute definition order

		~MeshPrimitive(){ Destroy(); }

		void Bind(bool doBind) const;
		void Destroy();
		
		// Getters/Setters:
		inline MeshPrimitiveParams& GetMeshParams() { return m_params; }
		inline MeshPrimitiveParams const& GetMeshParams() const { return m_params; }

		inline std::shared_ptr<gr::Material> MeshMaterial() { return m_meshMaterial; }
		inline std::shared_ptr<gr::Material const> const MeshMaterial() const { return m_meshMaterial; }

		inline gr::Transform*& GetOwnerTransform() { return m_ownerTransform; }
		inline gr::Transform const* GetOwnerTransform() const { return m_ownerTransform; }

		inline Bounds& GetLocalBounds() { return m_localBounds; }
		inline Bounds const& GetLocalBounds() const { return m_localBounds; }

		inline std::vector<float> const& GetPositions() const { return m_positions; }
		inline std::vector<float> const& GetNormals() const { return m_normals; }
		inline std::vector<float> const& GetColors() const { return m_colors; }
		inline std::vector<float> const& GetUV0() const { return m_uv0; }
		inline std::vector<float> const& GetTangents() const { return m_tangents; }
		inline std::vector<uint32_t> const& GetIndices() { return m_indices; }

		inline size_t NumIndices() const { return m_indices.size(); }

		inline std::unique_ptr<platform::MeshPrimitive::PlatformParams>& GetPlatformParams() { return m_platformParams; }
		inline std::unique_ptr<platform::MeshPrimitive::PlatformParams> const& GetPlatformParams() const { return m_platformParams; }

	private:		
		MeshPrimitiveParams m_params;

		std::shared_ptr<gr::Material> m_meshMaterial;

		// API-specific mesh params:
		std::unique_ptr<platform::MeshPrimitive::PlatformParams> m_platformParams;

		// Vertex data streams:
		std::vector<float> m_positions;		// vec3
		std::vector<float> m_normals;		// vec3
		std::vector<float> m_colors;		// vec4
		std::vector<float> m_uv0;			// vec2
		std::vector<float> m_tangents;		// vec4

		std::vector<uint32_t> m_indices;

		gr::Transform* m_ownerTransform;

		// TODO: Move bounds to the Mesh object
		Bounds m_localBounds; // MeshPrimitive bounds, in local space		
		void ComputeBounds(); // Computes m_localBounds

		void ComputeDataHash() override;

	private:
		// Constructing a mesh primitive modifies the GPU state; disallow all copy/move semantics
		MeshPrimitive() = delete;
		MeshPrimitive(MeshPrimitive const& rhs) = delete;
		MeshPrimitive(MeshPrimitive&& rhs) noexcept = delete;
		MeshPrimitive& operator=(MeshPrimitive const& rhs) = delete;
		MeshPrimitive& operator=(MeshPrimitive&& rhs) = delete;
	};
}


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
}


