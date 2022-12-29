// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "mikktspace.h"

#include "MeshPrimitive.h"


// Enable this if the UV (0,0) origin is in the top-left of the image. It will negate the sign packed into tangent.w
#define UPPER_LEFT_UV_ORIGIN

namespace util
{

class VertexAttributeBuilder
{
	public:
		struct MeshData
		{
			std::string const& m_name; // For debug spew...
			re::MeshPrimitive::MeshPrimitiveParams const* m_meshParams;
			std::vector<uint32_t>* m_indices;
			std::vector<glm::vec3>* m_positions;
			std::vector<glm::vec3>* m_normals;			// Created as face normals if empty
			std::vector<glm::vec4>* m_tangents;			// Computed from normals and UVs
			std::vector<glm::vec2>* m_UV0;				// Created as simple triangle UVs if empty
			std::vector<glm::vec4>* m_colors;			// Filled with (1,1,1,1) if empty

			std::vector<glm::tvec4<uint8_t>>* m_joints; // OPTIONAL: Ignored if empty
			std::vector<glm::vec4>* m_weights;			// OPTIONAL: Ignored if empty
		};

	public:
		static void BuildMissingVertexAttributes(MeshData* meshData);


	private:
		VertexAttributeBuilder();
		void ConstructMissingVertexAttributes(MeshData* meshData);
	
		void RemoveDegenerateTriangles(MeshData* meshData);
		void BuildSimpleTriangleUVs(MeshData* meshData);
		void BuildFlatNormals(MeshData* meshData);
		void SplitSharedAttributes(MeshData* meshData);
		void WeldTriangles(MeshData* meshData);

		// Optional vertex attributes:
		bool m_hasJoints;
		bool m_hasWeights;

		// Helpers for MikkTSpace:
		static int GetVertexIndex(const SMikkTSpaceContext* m_context, int faceIdx, int vertIdx);
		static int GetNumFaces(const SMikkTSpaceContext* m_context);
		static int GetNumFaceVerts(const SMikkTSpaceContext* m_context, int faceIdx);
		static void GetPosition(const SMikkTSpaceContext* m_context, float outpos[], int faceIdx, int vertIdx);
		static void GetNormal(const SMikkTSpaceContext* m_context, float outnormal[], int faceIdx, int vertIdx);
		static void GetTexCoords(const SMikkTSpaceContext* m_context, float outuv[], int faceIdx, int vertIdx);

		static void SetTangentSpaceBasic(
			const SMikkTSpaceContext* m_context, const float tangentu[], float fSign, int faceIdx, int vertIdx);

		SMikkTSpaceInterface m_interface{};
		SMikkTSpaceContext m_context{};
	};
}