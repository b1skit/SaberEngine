#pragma once

#include <string>
#include <memory>

#include "Transform.h"
#include "Material.h"
#include "Mesh_Platform.h"


namespace gr
{
	// Bounds of a mesh, scene, etc
	class Bounds
	{
	public:
		Bounds() :
			m_xMin(std::numeric_limits<float>::max()),
			m_xMax(-std::numeric_limits<float>::max()), // Note: -max is the furthest away from max
			m_yMin(std::numeric_limits<float>::max()),
			m_yMax(-std::numeric_limits<float>::max()),
			m_zMin(std::numeric_limits<float>::max()),
			m_zMax(-std::numeric_limits<float>::max())
		{
		}

		Bounds(Bounds const& rhs) = default;

		// TODO: Ensure our values give a valid 3D bounds? (ie. ?min != ?max)
		inline float& xMin() { return m_xMin; }
		inline float xMin() const { return m_xMin; }
		
		inline float& xMax() { return m_xMax; }
		inline float xMax() const { return m_xMax; }

		inline float& yMin() { return m_yMin; }
		inline float yMin() const { return m_yMin; }

		inline float& yMax() { return m_yMax; }
		inline float yMax() const { return m_yMax; }

		inline float& zMin() { return m_zMin; }
		inline float zMin() const { return m_zMin; }

		inline float& zMax() { return m_zMax; }
		inline float zMax() const { return m_zMax; }

		// Returns a Bounds, transformed from local space using transform
		Bounds GetTransformedBounds(glm::mat4 const& m_transform);

		void Make3Dimensional();

	private:
		float m_xMin;
		float m_xMax;
		float m_yMin;
		float m_yMax;
		float m_zMin;
		float m_zMax;
	};


	class Mesh
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

		struct MeshParams
		{
			DrawMode m_drawMode = DrawMode::Triangles;
		};

	public:
		Mesh(std::string const& name,
			std::vector<float>& positions,
			std::vector<float>& normals,
			std::vector<float>& colors,
			std::vector<float>& uv0,
			std::vector<float>& tangents,
			std::vector<uint32_t>& indices,
			std::shared_ptr<gr::Material> material,
			gr::Mesh::MeshParams const& meshParams);
		
		// ^^^^^^^^TODO: Force a parent Transform* here???????????????
		// TODO: Rearrange these args to match shader vertex attribute definition order

		// Constructing a mesh modifies the GPU state; disallow all move semantics for now
		Mesh() = delete;
		Mesh(Mesh const& rhs) = delete;
		Mesh(Mesh&& rhs) noexcept = delete;
		Mesh& operator=(Mesh const& rhs) = delete;
		Mesh& operator=(Mesh&& rhs) = delete;

		~Mesh(){ Destroy(); }

		void Bind(bool doBind);
		void Destroy();
		
		// Getters/Setters:
		inline std::string const& Name() { return m_name; }

		inline MeshParams& GetMeshParams() { return m_params; }
		inline MeshParams const& GetMeshParams() const { return m_params; }

		inline std::shared_ptr<gr::Material> MeshMaterial() { return m_meshMaterial; }
		inline std::shared_ptr<gr::Material const> const MeshMaterial() const { return m_meshMaterial; }

		inline gr::Transform& GetTransform() { return m_transform; }
		inline gr::Transform const& GetTransform() const { return m_transform; }

		inline Bounds& GetLocalBounds() { return m_localBounds; }
		inline Bounds const& GetLocalBounds() const { return m_localBounds; }

		inline std::vector<float> const& GetPositions() const { return m_positions; }
		inline std::vector<float> const& GetNormals() const { return m_normals; }
		inline std::vector<float> const& GetColors() const { return m_colors; }
		inline std::vector<float> const& GetUV0() const { return m_uv0; }
		inline std::vector<float> const& GetTangents() const { return m_tangents; }
		inline std::vector<uint32_t> const& GetIndices() { return m_indices; }

		inline size_t NumIndices() const { return m_indices.size(); }

		inline std::unique_ptr<platform::Mesh::PlatformParams>& GetPlatformParams() { return m_platformParams; }
		inline std::unique_ptr<platform::Mesh::PlatformParams> const& GetPlatformParams() const { return m_platformParams; }

	private:
		const std::string m_name;
		
		MeshParams m_params;

		std::shared_ptr<gr::Material> m_meshMaterial;

		// API-specific mesh params:
		std::unique_ptr<platform::Mesh::PlatformParams> m_platformParams;

		// Vertex data streams:
		std::vector<float> m_positions;		// vec3
		std::vector<float> m_normals;		// vec3
		std::vector<float> m_colors;		// vec4
		std::vector<float> m_uv0;			// vec2
		std::vector<float> m_tangents;		// vec4

		std::vector<uint32_t> m_indices;

		gr::Transform m_transform; // TODO: Remove this once we're using GLTF

		// TODO: Move mesh bounds to the RenderMesh object
		Bounds m_localBounds; // Mesh bounds, in local space		
		void ComputeBounds(); // Computes m_localBounds
	};

	/******************************************************************************************************************/

	namespace meshfactory
	{
		extern std::shared_ptr<Mesh> CreateCube(std::shared_ptr<gr::Material> newMeshMaterial = nullptr);

		extern std::shared_ptr<Mesh> CreateQuad(
			glm::vec3 tl /*= vec3(-0.5f, 0.5f, 0.0f)*/,
			glm::vec3 tr /*= vec3(0.5f, 0.5f, 0.0f)*/,
			glm::vec3 bl /*= vec3(-0.5f, -0.5f, 0.0f)*/,
			glm::vec3 br /*= vec3(0.5f, -0.5f, 0.0f)*/,
			std::shared_ptr<gr::Material> newMeshMaterial = nullptr);

		extern std::shared_ptr<Mesh> CreateSphere(
			float radius = 0.5f,
			size_t numLatSlices = 16,
			size_t numLongSlices = 16,
			std::shared_ptr<gr::Material> newMeshMaterial = nullptr);
	}
}



