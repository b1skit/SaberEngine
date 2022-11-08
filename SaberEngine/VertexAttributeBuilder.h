#pragma once

#include <vector>
#include <string>

#include "mikktspace.h"
#include <glm/glm.hpp>

#include "MeshPrimitive.h"


namespace util
{

class VertexAttributeBuilder
{
	public:
		struct MeshData
		{
		std::string const& m_name; // For debug spew...
		gr::MeshPrimitive::MeshPrimitiveParams* m_meshParams;
		std::vector<uint32_t>* m_indices;
		std::vector<glm::vec3>* m_positions;
		std::vector<glm::vec3>* m_normals;
		std::vector<glm::vec2>* m_UV0;
		std::vector<glm::vec4>* m_tangents;
		};

	public:
		VertexAttributeBuilder();
		void ConstructMissingVertexAttributes(MeshData* meshData);

	private:
		void RemoveDegenerateTriangles(MeshData* meshData);
		void BuildSimpleTriangleUVs(MeshData* meshData);
		void BuildFlatNormals(MeshData* meshData);
		void RemoveTriangleIndexing(MeshData* meshData);
		void WeldUnindexedTriangles(MeshData* meshData);

		// Helpers for MikkTSpace:
		static int GetVertexIndex(const SMikkTSpaceContext* m_context, int faceIdx, int vertIdx);
		static int GetNumFaces(const SMikkTSpaceContext* m_context);
		static int GetNumFaceVerts(const SMikkTSpaceContext* m_context, int faceIdx);
		static void GetPosition(const SMikkTSpaceContext* m_context, float outpos[], int faceIdx, int vertIdx);
		static void GetNormal(const SMikkTSpaceContext* m_context, float outnormal[], int faceIdx, int vertIdx);
		static void GetTexCoords(const SMikkTSpaceContext* m_context, float outuv[], int faceIdx, int vertIdx);

		static void SetTangentSpaceBasic(const SMikkTSpaceContext* m_context, const float tangentu[], float fSign, int faceIdx, int vertIdx);

		SMikkTSpaceInterface m_interface{};
		SMikkTSpaceContext m_context{};
	};
}