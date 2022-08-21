#pragma once

#include <string>
#include <memory>

#include "Transform.h"
using SaberEngine::Transform;

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
		inline float& xMax() { return m_xMax; }
		inline float& yMin() { return m_yMin; }
		inline float& yMax() { return m_yMax; }
		inline float& zMin() { return m_zMin; }
		inline float& zMax() { return m_zMax; }

		// Returns a Bounds, transformed from local space using transform
		Bounds GetTransformedBounds(mat4 const& m_transform);

		void Make3Dimensional();

	private:
		float m_xMin;
		float m_xMax;
		float m_yMin;
		float m_yMax;
		float m_zMin;
		float m_zMax;
	};


	struct Vertex
	{
	public:
		Vertex() :
			m_position(0.0f, 0.0f, 0.0f),
			m_tangent(1.0f, 0.0f, 0.0f),
			m_normal(0.0f, 1.0f, 0.0f),
			m_bitangent(0.0f, 0.0f, 1.0f),
			m_color(0.0f, 0.0f, 0.0f, 0.0f),
			m_uv0(0.0f, 0.0f, 0.0f, 0.0f),
			m_uv1(0.0f, 0.0f, 0.0f, 0.0f),
			m_uv2(0.0f, 0.0f, 0.0f, 0.0f),
			m_uv3(0.0f, 0.0f, 0.0f, 0.0f)
		{
		}

		//Explicit constructor:
		Vertex(glm::vec3 const& position,
			glm::vec3 const& normal,
			glm::vec3 const tangent,
			glm::vec3 const bitangent,
			glm::vec4 const& color,
			const vec4& uv0) :
			m_position(position),
			m_normal(normal),
			m_tangent(tangent),
			m_bitangent(bitangent),
			m_color(color),
			m_uv0(uv0),
			m_uv1(0.0f, 0.0f, 0.0f, 0.0f), // Just set these to 0 for now...
			m_uv2(0.0f, 0.0f, 0.0f, 0.0f),
			m_uv3(0.0f, 0.0f, 0.0f, 0.0f)
		{

		}

		glm::vec3 m_position;
		glm::vec4 m_color;

		glm::vec3 m_normal;
		glm::vec3 m_tangent;
		glm::vec3 m_bitangent;

		glm::vec4 m_uv0;
		glm::vec4 m_uv1;
		glm::vec4 m_uv2;
		glm::vec4 m_uv3;

	protected:


	private:


	};


	class Mesh
	{
	public:
		Mesh(string name, 
			std::vector<gr::Vertex> vertices, 
			std::vector<uint32_t> indices, 
			std::shared_ptr<SaberEngine::Material> newMeshMaterial);

		// Constructing a mesh modifies the GPU state; disallow all move semantics for now
		Mesh() = delete;
		Mesh(Mesh const& rhs) = delete;
		Mesh(Mesh&& rhs) noexcept = delete;
		Mesh& operator=(Mesh const& rhs) = delete;
		Mesh& operator=(Mesh&& rhs) = delete;

		void Bind(bool doBind);
		void Destroy();
		
		// Getters/Setters:
		inline std::string& Name() { return meshName; }

		inline std::vector<Vertex>& Vertices() { return m_vertices; }
		inline size_t NumVerts() const { return m_vertices.size(); }
				
		inline std::vector<uint32_t>&	Indices() { return m_indices; }
		inline size_t NumIndices() const { return m_indices.size(); }

		inline std::shared_ptr<SaberEngine::Material> MeshMaterial() { return m_meshMaterial; }

		inline Transform& GetTransform() { return m_transform; }

		inline Bounds& GetLocalBounds() { return m_localBounds; }

		inline std::unique_ptr<platform::Mesh::PlatformParams>& GetPlatformParams() { return m_platformParams; }


	private:
		// Computes mesh localBounds, in local space
		void ComputeBounds();
		Bounds m_localBounds;	// Mesh localBounds, in local space

		std::vector<Vertex> m_vertices;
		std::vector<uint32_t> m_indices;

		std::shared_ptr<SaberEngine::Material> m_meshMaterial = nullptr;

		Transform m_transform;
		std::string meshName = "UNNAMED_MESH";

		// API-specific mesh params:
		std::unique_ptr<platform::Mesh::PlatformParams> m_platformParams;
	};

	/******************************************************************************************************************/

	namespace meshfactory
	{
		extern std::shared_ptr<Mesh> CreateCube(std::shared_ptr<SaberEngine::Material> newMeshMaterial = nullptr);

		extern std::shared_ptr<Mesh> CreateQuad(
			glm::vec3 tl /*= vec3(-0.5f, 0.5f, 0.0f)*/,
			glm::vec3 tr /*= vec3(0.5f, 0.5f, 0.0f)*/,
			glm::vec3 bl /*= vec3(-0.5f, -0.5f, 0.0f)*/,
			glm::vec3 br /*= vec3(0.5f, -0.5f, 0.0f)*/,
			std::shared_ptr<SaberEngine::Material> newMeshMaterial = nullptr);

		extern std::shared_ptr<Mesh> CreateSphere(float radius = 0.5f,
			size_t numLatSlices = 16,
			size_t numLongSlices = 16,
			std::shared_ptr<SaberEngine::Material> newMeshMaterial = nullptr);
	}
}



