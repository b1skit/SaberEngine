// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "MeshPrimitive.h"
#include "VertexStream.h"

#include "Core/Util/ByteVector.h"

#include "mikktspace.h"


// Enable this if the UV (0,0) origin is in the top-left of the image. It will negate the sign packed into tangent.w
#define UPPER_LEFT_UV_ORIGIN

namespace grutil
{
	class VertexStreamBuilder
	{
	public:
		struct MeshData
		{
			std::string const& m_name; // For debug spew...
			gr::MeshPrimitive::MeshPrimitiveParams const* m_meshParams = nullptr;

			util::ByteVector* m_indices = nullptr;		// uint32_t
			gr::VertexStream::StreamDesc* m_indicesStreamDesc = nullptr; // Required if we change the data type

			util::ByteVector* m_positions = nullptr;	// glm::vec3 (Note: Cannot be built)

			util::ByteVector* m_normals = nullptr;		// glm::vec3: Created as face normals if empty
			util::ByteVector* m_tangents = nullptr;		// glm::vec4: Computed from normals and UVs
			util::ByteVector* m_UV0 = nullptr;			// glm::vec2: Created as simple triangle UVs if empty

			// Streams that just need to be reordered: Morph displacements/colors/weights/uv1+ etc
			std::vector<util::ByteVector*>* m_extraChannels;
		};

	public:
		static void BuildMissingVertexAttributes(MeshData*);


	private:
		VertexStreamBuilder();
		void ConstructMissingVertexAttributes(MeshData*);
	
		void RemoveDegenerateTriangles(MeshData*);
		void BuildIndexList(MeshData*);
		void BuildSimpleTriangleUVs(MeshData*);
		void BuildFlatNormals(MeshData*);
		void SplitSharedAttributes(MeshData*);
		void WeldTriangles(MeshData*);

		// Optional vertex attributes:
		bool m_canBuildNormals;
		bool m_canBuildTangents;
		bool m_canBuildUVs;

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