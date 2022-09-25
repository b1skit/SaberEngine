#include <string>

#include "mikktspace.c" // LNK2019 unresolved external symbol genTangSpaceDefault otherwise...

#include <glm/glm.hpp>

#include "TangentBuilder.h"
#include "DebugConfiguration.h"

using std::string;
using std::to_string;
using glm::vec2;
using glm::vec3;
using glm::vec4;


namespace util
{
	TangentBuilder::TangentBuilder()
	{
		m_interface.m_getNumFaces			= GetNumFaces;
		m_interface.m_getNumVerticesOfFace  = GetNumFaceVerts;
		m_interface.m_getNormal				= GetNormal;
		m_interface.m_getPosition			= GetPosition;
		m_interface.m_getTexCoord			= GetTexCoords;

		m_interface.m_setTSpaceBasic		= SetTangentSpaceBasic;

		m_context.m_pInterface = &m_interface;
	}


	void TangentBuilder::ConstructMeshTangents(util::TangentBuilder::MeshData* meshData)
	{
		// TODO: Detect indexed triangle lists, remove it, and re-weld the result

		m_context.m_pUserData = meshData;

		if (meshData->m_positions->size() > 0 &&
			meshData->m_normals->size() > 0 &&
			meshData->m_tangents->size() > 0 &&
			meshData->m_UV0->size() > 0 &&
			meshData->m_indices->size() > 0 &&
			meshData->m_meshParams != nullptr
			)
		{
			LOG("Computing tangents for mesh %s", meshData->m_name.c_str());

			tbool result = genTangSpaceDefault(&this->m_context);
			SEAssert("Failed to generate tangents", result);
		}
		else
		{
			LOG_WARNING("Could not generate tangents for mesh %s, required mesh data incomplete", meshData->m_name);
		}
	}


	void TangentBuilder::RemoveTriangleIndexing(MeshData* meshData)
	{
		// TODO...
	}


	int TangentBuilder::GetNumFaces(const SMikkTSpaceContext* m_context)
	{
		MeshData* meshData = static_cast<MeshData*> (m_context->m_pUserData);
		
		float floatSize = (float)meshData->m_indices->size() / 3.0f;
		int intSize = (int)meshData->m_indices->size() / 3;

		SEAssert("Unexpected number of indexes", (floatSize - (float)intSize) == 0.f);

		return intSize;
	}

	int TangentBuilder::GetNumFaceVerts(const SMikkTSpaceContext* m_context, const int faceIdx)
	{
		MeshData* meshData = static_cast<MeshData*> (m_context->m_pUserData);

		SEAssert("Only triangular faces are currently supported", 
			meshData->m_meshParams->m_drawMode == gr::Mesh::DrawMode::Triangles);
		
		return 3;
	}

	void TangentBuilder::GetPosition(
		const SMikkTSpaceContext* m_context, float* outpos, const int faceIdx, const int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		int index = GetVertexIndex(m_context, faceIdx, vertIdx);
		vec3 position = meshData->m_positions->at(index);

		outpos[0] = position.x;
		outpos[1] = position.y;
		outpos[2] = position.z;
	}


	void TangentBuilder::GetNormal(
		const SMikkTSpaceContext* m_context, float* outnormal, const int faceIdx, const int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		int index = GetVertexIndex(m_context, faceIdx, vertIdx);
		vec3 normal = meshData->m_normals->at(index);;

		outnormal[0] = normal.x;
		outnormal[1] = normal.y;
		outnormal[2] = normal.z;
	}


	void TangentBuilder::GetTexCoords(
		const SMikkTSpaceContext* m_context, float* outuv, const int faceIdx, const int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		auto index = GetVertexIndex(m_context, faceIdx, vertIdx);
		vec2 uv = meshData->m_UV0->at(index);

		outuv[0] = uv.x;
		outuv[1] = uv.y;
	}


	void TangentBuilder::SetTangentSpaceBasic(
		const SMikkTSpaceContext* m_context, const float* tangentu, const float fSign, const int faceIdx, const int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		int index = GetVertexIndex(m_context, faceIdx, vertIdx);
		vec4* tangent = &meshData->m_tangents->at(index);

		tangent->x = tangentu[0];
		tangent->y = tangentu[1];
		tangent->z = tangentu[2];
		tangent->w = fSign;
	}


	int TangentBuilder::GetVertexIndex(const SMikkTSpaceContext* m_context, int faceIdx, int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		int faceSize = GetNumFaceVerts(m_context, faceIdx); // Currently only 3 supported...
		int indicesIdx = (faceIdx * faceSize) + vertIdx;
		int index = meshData->m_indices->at(indicesIdx);

		return index;
	}
}
