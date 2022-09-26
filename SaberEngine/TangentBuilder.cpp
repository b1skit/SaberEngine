#include <string>

#include "mikktspace.c" // LNK2019 otherwise...
#include "weldmesh.h"
#include "weldmesh.c" // LNK2019 otherwise...

#include <glm/glm.hpp>

#include "TangentBuilder.h"
#include "DebugConfiguration.h"
#include "CoreEngine.h"

using en::CoreEngine;
using std::string;
using std::to_string;
using std::vector;
using std::move;
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


	void TangentBuilder::ConstructMeshTangents(MeshData* meshData)
	{
		LOG("Building tangents for mesh \"%s\" from %d vertices", meshData->m_name.c_str(), meshData->m_positions->size());

		SEAssert("Cannot pass null data", 
			meshData->m_meshParams && meshData->m_indices && meshData->m_positions && 
			meshData->m_normals && meshData->m_UV0 && meshData->m_tangents);

		// Allocate space for our tangents. We'll re-weld at the end, so allocate to match the number of indices
		SEAssert("Expected an empty tangents vector", meshData->m_tangents->size() == 0);
		meshData->m_tangents->resize(meshData->m_indices->size(), vec4(0, 0, 0, 0)); // Zeros for now...

		// Build UVs if none exist:
		if (meshData->m_UV0->size() == 0)
		{
			LOG("Mesh \"%s\" is missing UVs, adding a simple default set", meshData->m_name.c_str());
			BuildSimpleTriangleUVs(meshData);
		}

		// Convert indexed triangle lists to non-indexed:
		bool removedIndexing = false;
		if (meshData->m_indices->size() > meshData->m_positions->size())
		{
			LOG("Mesh \"%s\" uses triangle indexing, de-indexing...", meshData->m_name.c_str());
			RemoveTriangleIndexing(meshData);
			removedIndexing = true;
		}

		m_context.m_pUserData = meshData;

		if (meshData->m_positions->size() > 0 &&
			meshData->m_normals->size() > 0 &&
			meshData->m_UV0->size() > 0 &&
			meshData->m_tangents->size() > 0 &&
			meshData->m_indices->size() > 0 &&
			meshData->m_meshParams != nullptr
			)
		{
			LOG("Computing tangents for mesh \"%s\"", meshData->m_name.c_str());

			tbool result = genTangSpaceDefault(&this->m_context);
			SEAssert("Failed to generate tangents", result);
		}
		else
		{
			SEAssert("Required mesh data is incomplete or missing elements. Cannot generate tangents", false);
		}

		// Re-index the result, if required:
		if (removedIndexing)
		{
			LOG("Re-welding vertices to build unique vertex index list for mesh \"%s\"", meshData->m_name.c_str());
			WeldUnindexedTriangles(meshData);
		}

		LOG("Mesh \"%s\" now has %d unique vertices", meshData->m_name.c_str(), meshData->m_positions->size());
	}


	void TangentBuilder::BuildSimpleTriangleUVs(MeshData* meshData)
	{
		platform::RenderingAPI const& api =
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();
		const bool botLeftZeroZero = api == platform::RenderingAPI::OpenGL ? true : false;

		// Build simple, overlapping UVs, placing the vertices of every triangle in the TL, BL, BR corners of UV space:
		vec2 TL, BL, BR;
		if (botLeftZeroZero) // OpenGL-style: (0,0) in the bottom-left of UV space
		{
			TL = vec2(0, 1);
			BL = vec2(0, 0);
			BR = vec2(1, 0);

		}
		else // D3D-style: (0,0) in the top-left of UV space
		{
			TL = vec2(0, 0);
			BL = vec2(0, 1);
			BR = vec2(1, 1);
		}

		SEAssert("Invalid index array length", meshData->m_indices->size() % 3 == 0);

		// Allocate our vector to ensure it's the correct size:
		meshData->m_UV0->resize(meshData->m_positions->size(), vec2(0, 0));

		for (size_t i = 0; i < meshData->m_indices->size(); i += 3)
		{
			meshData->m_UV0->at(meshData->m_indices->at(i)) = TL;
			meshData->m_UV0->at(meshData->m_indices->at(i + 1)) = BL;
			meshData->m_UV0->at(meshData->m_indices->at(i + 2)) = BR;
		}
	}


	void TangentBuilder::RemoveTriangleIndexing(MeshData* meshData)
	{
		SEAssert("Expected tangents have already been allocated", 
			meshData->m_tangents->size() == meshData->m_indices->size());

		// Use our indices to unpack duplicated vertex attributes:
		vector<uint32_t> newIndices(meshData->m_indices->size(), 0);
		vector<vec3> newPositions(meshData->m_indices->size(), vec3(0, 0, 0));
		vector<vec3> newNormals(meshData->m_indices->size(), vec3(0, 0, 0));
		vector<vec2> newUVs(meshData->m_indices->size(), vec2(0, 0));
		for (size_t i = 0; i < meshData->m_indices->size(); i++)
		{
			newIndices[i] = (uint32_t)i;
			newPositions[i] = meshData->m_positions->at(meshData->m_indices->at(i));
			newNormals[i] = meshData->m_normals->at(meshData->m_indices->at(i));
			newUVs[i] = meshData->m_UV0->at(meshData->m_indices->at(i));
		}

		*meshData->m_indices = move(newIndices);
		*meshData->m_positions = move(newPositions);
		*meshData->m_normals = move(newNormals);
		*meshData->m_UV0 = move(newUVs);
	}


	void TangentBuilder::WeldUnindexedTriangles(MeshData* meshData)
	{
		SEAssert("Mikktspace operates on system's int, SaberEngine operates on explicit 32-bit uints", 
			sizeof(int) == sizeof(uint32_t));

		// The Mikktspace welder expects tightly-packed vertex data; Pack it to get the index list, then reorder our
		// individual streams once welding is complete
		auto PackAttribute = [](
			void* src, void* dest, size_t byteOffset, size_t strideBytes, size_t numElements, size_t elementBytes)
		{
			for (size_t i = 0; i < numElements; i++)
			{
				void* currentSrc = (uint8_t*)src + (elementBytes * i);
				void* currentDest = (uint8_t*)dest + byteOffset + (strideBytes * i);
				
				std::memcpy(currentDest, currentSrc, elementBytes);
			}
		};

		// piRemapTable: iNrVerticesIn * sizeof(int)
		vector<int> remapTable(meshData->m_positions->size(), 0); // This will contain our final indexes

		// We'll pack our vertex attributes together into blocks of floats:
		const size_t floatsPerVertex = (sizeof(vec3) + sizeof(vec3) + sizeof(vec2) + sizeof(vec4)) / sizeof(float);
		SEAssert("Data size mismatch/miscalulation", floatsPerVertex == 12);

		// pfVertexDataOut: iNrVerticesIn * iFloatsPerVert * sizeof(float)
		const size_t numElements = meshData->m_positions->size();
		const size_t vertexStrideBytes = floatsPerVertex * sizeof(float);
		const size_t numVertexBytesOut = numElements * vertexStrideBytes;
		vector<float> vertexDataOut(numVertexBytesOut, 0); // Will contain only unique vertices after welding

		// pfVertexDataIn: Our tightly-packed vertex data:
		vector<float> packedVertexData(meshData->m_positions->size() * floatsPerVertex, 0);		

		const size_t strideSizeInBytes = floatsPerVertex * sizeof(float);
		size_t byteOffset = 0;
		PackAttribute(
			(float*)meshData->m_positions->data(), 
			packedVertexData.data(), 
			byteOffset,
			strideSizeInBytes,
			numElements,
			sizeof(vec3));	// Position = vec3
		byteOffset += sizeof(vec3);
		
		PackAttribute(
			(float*)meshData->m_normals->data(),
			packedVertexData.data(),
			byteOffset,
			strideSizeInBytes,
			numElements,
			sizeof(vec3));	// Normals = vec3
		byteOffset += sizeof(vec3);

		PackAttribute(
			(float*)meshData->m_UV0->data(),
			packedVertexData.data(),
			byteOffset,
			strideSizeInBytes,
			numElements,
			sizeof(vec2));	// UV0 = vec2
		byteOffset += sizeof(vec2);

		PackAttribute(
			(float*)meshData->m_tangents->data(),
			packedVertexData.data(),
			byteOffset,
			strideSizeInBytes,
			numElements,
			sizeof(vec4));	// tangents = vec4
		byteOffset += sizeof(vec4);

		// Weld the verts to obtain our final unique indexing:
		const int numUniqueVertsFound = 
			WeldMesh(remapTable.data(), vertexDataOut.data(), packedVertexData.data(), (int)numElements, (int)floatsPerVertex);

		// Repack existing data streams according to the updated indexes:
		meshData->m_indices->resize(remapTable.size());
		meshData->m_positions->resize(numUniqueVertsFound);
		meshData->m_normals->resize(numUniqueVertsFound);
		meshData->m_UV0->resize(numUniqueVertsFound);
		meshData->m_tangents->resize(numUniqueVertsFound);
		for (size_t i = 0; i < remapTable.size(); i++)
		{
			const int vertexIndex = remapTable[i];
			meshData->m_indices->at(i) = (uint32_t)vertexIndex;

			// Pointer to the first byte in our blob of interleaved vertex data:
			const uint8_t* currentVertStart = (uint8_t*)vertexDataOut.data() + ((size_t)vertexIndex * vertexStrideBytes);

			// Copy each element back into its individual data stream:
			uint32_t packedVertByteOffset = 0;
			memcpy(&meshData->m_positions->at(vertexIndex).x, currentVertStart + packedVertByteOffset, sizeof(vec3));
			packedVertByteOffset += sizeof(vec3);

			memcpy(&meshData->m_normals->at(vertexIndex).x, currentVertStart + packedVertByteOffset, sizeof(vec3));
			packedVertByteOffset += sizeof(vec3);

			memcpy(&meshData->m_UV0->at(vertexIndex).x, currentVertStart + packedVertByteOffset, sizeof(vec2));
			packedVertByteOffset += sizeof(vec2);

			memcpy(&meshData->m_tangents->at(vertexIndex).x, currentVertStart + packedVertByteOffset, sizeof(vec4));
			packedVertByteOffset += sizeof(vec4);
		}
	}


	int TangentBuilder::GetNumFaces(const SMikkTSpaceContext* m_context)
	{
		MeshData* meshData = static_cast<MeshData*> (m_context->m_pUserData);
		
		SEAssert("Unexpected number of indexes. Expected an exact factor of 3", meshData->m_indices->size() % 3 == 0);

		return (int)meshData->m_indices->size() / 3;
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
