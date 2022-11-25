#include <string>

#include "mikktspace.c" // LNK2019 otherwise...
#include "weldmesh.h"
#include "weldmesh.c" // LNK2019 otherwise...

#include <glm/glm.hpp>

#include "VertexAttributeBuilder.h"
#include "DebugConfiguration.h"
#include "Config.h"

using en::Config;
using std::string;
using std::to_string;
using std::vector;
using std::move;
using glm::vec2;
using glm::vec3;
using glm::vec4;


namespace util
{
	void VertexAttributeBuilder::BuildMissingVertexAttributes(MeshData* meshData)
	{
		util::VertexAttributeBuilder tangentBuilder;
		tangentBuilder.ConstructMissingVertexAttributes(meshData);
	}


	VertexAttributeBuilder::VertexAttributeBuilder()
	{
		m_interface.m_getNumFaces			= GetNumFaces;
		m_interface.m_getNumVerticesOfFace  = GetNumFaceVerts;
		m_interface.m_getNormal				= GetNormal;
		m_interface.m_getPosition			= GetPosition;
		m_interface.m_getTexCoord			= GetTexCoords;
		m_interface.m_setTSpaceBasic		= SetTangentSpaceBasic;

		m_context.m_pInterface = &m_interface;
	}


	void VertexAttributeBuilder::ConstructMissingVertexAttributes(MeshData* meshData)
	{
		LOG("Processing mesh \"%s\" with %d vertices...", meshData->m_name.c_str(), meshData->m_positions->size());

		SEAssert("Cannot pass null data. If an attribute does not exist, a vector of size 0 is expected.", 
			meshData->m_meshParams && meshData->m_indices && meshData->m_positions && 
			meshData->m_normals && meshData->m_UV0 && meshData->m_tangents);

		const bool isIndexed = meshData->m_indices->size() > meshData->m_positions->size();
		const bool hasUVs = !meshData->m_UV0->empty();
		const bool hasNormals = !meshData->m_normals->empty();
		bool hasTangents = !meshData->m_tangents->empty();

		if (hasUVs && hasNormals && hasTangents)
		{
			LOG("Mesh \"%s\" has all required attributes", meshData->m_name.c_str());
			return; // Note: We skip degenerate triangle removal this way, but low risk as the asset came with all attribs
		}

		// Allocate space for any missing attributes:
		const size_t numVerts = meshData->m_indices->size(); // Assume triangle lists: 3 index entries per triangle
		if (!hasUVs)
		{
			meshData->m_UV0->resize(numVerts, vec2(0, 0));
		}
		if (!hasNormals)
		{
			meshData->m_normals->resize(numVerts, vec3(0, 0, 0));

			if (hasTangents)
			{
				// GLTF 2.0 specs: When normals are not specified, client implementations MUST calculate flat normals 
				// and the provided tangents(if present) MUST be ignored.
				// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview
				meshData->m_tangents->clear();
				meshData->m_tangents->resize(meshData->m_indices->size(), vec4(0, 0, 0, 0));
				hasTangents = false;
			}
		}
		if (!hasTangents)
		{
			meshData->m_tangents->resize(numVerts, vec4(0, 0, 0, 0));
		}

		// Convert indexed triangle lists to non-indexed:
		if (isIndexed)
		{
			LOG("MeshPrimitive \"%s\" uses triangle indexing, de-indexing...", meshData->m_name.c_str());
			RemoveTriangleIndexing(meshData);
		}

		// Find and remove any degenerate triangles:
		RemoveDegenerateTriangles(meshData);

		// Build any missing attributes:
		if (!hasUVs)
		{
			LOG("MeshPrimitive \"%s\" is missing UVs, generating a simple set...", meshData->m_name.c_str());
			BuildSimpleTriangleUVs(meshData);
		}
		if (!hasNormals)
		{
			LOG("MeshPrimitive \"%s\" is missing normals, flat normals will be generated...", meshData->m_name.c_str());

			BuildFlatNormals(meshData);
		}
		if (!hasTangents)
		{
			LOG("MeshPrimitive \"%s\" is missing tangents, they will be generated...", meshData->m_name.c_str());

			m_context.m_pUserData = meshData;
			tbool result = genTangSpaceDefault(&this->m_context);
			SEAssert("Failed to generate tangents", result);
		}

		// Re-index the result, if required:
		if (isIndexed)
		{
			LOG("Re-welding vertices to build unique vertex index list for mesh \"%s\"", meshData->m_name.c_str());
			WeldUnindexedTriangles(meshData);
		}

		LOG("MeshPrimitive \"%s\" now has %d unique vertices", meshData->m_name.c_str(), meshData->m_positions->size());
	}


	void VertexAttributeBuilder::RemoveDegenerateTriangles(MeshData* meshData)
	{
		SEAssert("Expected an un-indexed triangle list", 
			meshData->m_indices->size() % 3 == 0 &&
			meshData->m_positions->size() == meshData->m_indices->size() &&
			meshData->m_normals->size() == meshData->m_indices->size() &&
			meshData->m_UV0->size() == meshData->m_indices->size()&&
			meshData->m_tangents->size() == meshData->m_indices->size()
		);

		vector<uint32_t> newIndices;
		vector<vec3> newPositions;
		vector<vec3> newNormals;
		vector<vec2> newUVs;
		vector<vec4> newTangents;

		// We might remove verts, so reserve rather than resize...
		const size_t maxNumVerts = meshData->m_indices->size(); // Assume triangle lists: 3 index entries per triangle
		newIndices.reserve(maxNumVerts);
		newPositions.reserve(maxNumVerts);
		newNormals.reserve(maxNumVerts);
		newUVs.reserve(maxNumVerts);
		newTangents.reserve(maxNumVerts);

		size_t numDegeneratesFound = 0;
		uint32_t insertIdx = 0;
		for (size_t i = 0; i < meshData->m_indices->size(); i += 3)
		{
			const vec3& p0 = meshData->m_positions->at(meshData->m_indices->at(i));
			const vec3& p1 = meshData->m_positions->at(meshData->m_indices->at(i + 1));
			const vec3& p2 = meshData->m_positions->at(meshData->m_indices->at(i + 2));

			const vec3 v0 = p0 - p2;
			const vec3 v1 = p1 - p2;
			const vec3 v2 = p0 - p1;

			const float v0Length = glm::length(v0);
			const float v1Length = glm::length(v1);
			const float v2Length = glm::length(v2);

			const bool isValid =
				v0Length + v1Length > v2Length &&
				v0Length + v2Length > v1Length &&
				v1Length + v2Length > v0Length;

			if (isValid)
			{
				SEAssert("Insertions are out of sync", insertIdx == newPositions.size());

				newIndices.emplace_back(insertIdx);
				newIndices.emplace_back(insertIdx + 1);
				newIndices.emplace_back(insertIdx + 2);

				newPositions.emplace_back(meshData->m_positions->at(meshData->m_indices->at(i)));
				newPositions.emplace_back(meshData->m_positions->at(meshData->m_indices->at(i + 1)));
				newPositions.emplace_back(meshData->m_positions->at(meshData->m_indices->at(i + 2)));

				newNormals.emplace_back(meshData->m_normals->at(meshData->m_indices->at(i)));
				newNormals.emplace_back(meshData->m_normals->at(meshData->m_indices->at(i + 1)));
				newNormals.emplace_back(meshData->m_normals->at(meshData->m_indices->at(i + 2)));
				
				newUVs.emplace_back(meshData->m_UV0->at(meshData->m_indices->at(i)));
				newUVs.emplace_back(meshData->m_UV0->at(meshData->m_indices->at(i + 1)));
				newUVs.emplace_back(meshData->m_UV0->at(meshData->m_indices->at(i + 2)));

				newTangents.emplace_back(meshData->m_tangents->at(meshData->m_indices->at(i)));
				newTangents.emplace_back(meshData->m_tangents->at(meshData->m_indices->at(i + 1)));
				newTangents.emplace_back(meshData->m_tangents->at(meshData->m_indices->at(i + 2)));

				insertIdx += 3;
			}
			else
			{
				numDegeneratesFound++;
			}
		}

		*meshData->m_indices = move(newIndices);
		*meshData->m_positions = move(newPositions);
		*meshData->m_normals = move(newNormals);
		*meshData->m_UV0 = move(newUVs);
		*meshData->m_tangents = move(newTangents);

		if (numDegeneratesFound > 0)
		{
			LOG_WARNING("Removed %d degenerate triangles from mesh \"%s\"", numDegeneratesFound, meshData->m_name.c_str());
		}
	}


	void VertexAttributeBuilder::BuildFlatNormals(MeshData* meshData)
	{
		SEAssert("Expected a triangle list and pre-allocated normals vector", 
			meshData->m_indices->size() % 3 == 0 && meshData->m_normals->size() == meshData->m_indices->size());

		for (size_t i = 0; i < meshData->m_indices->size(); i += 3)
		{
			const vec3& p0 = meshData->m_positions->at(meshData->m_indices->at(i));
			const vec3& p1 = meshData->m_positions->at(meshData->m_indices->at(i + 1));
			const vec3& p2 = meshData->m_positions->at(meshData->m_indices->at(i + 2));

			const vec3 v0 = p0 - p2;
			const vec3 v1 = p1 - p2;

			const vec3 faceNormal = glm::normalize(glm::cross(v0, v1));
			
			meshData->m_normals->at(meshData->m_indices->at(i)) = faceNormal;
			meshData->m_normals->at(meshData->m_indices->at(i + 1)) = faceNormal;
			meshData->m_normals->at(meshData->m_indices->at(i + 2)) = faceNormal;
		}
	}


	void VertexAttributeBuilder::BuildSimpleTriangleUVs(MeshData* meshData)
	{
		SEAssert("Expected a triangle list and pre-allocated UV0 vector",
			meshData->m_indices->size() % 3 == 0 && meshData->m_UV0->size() == meshData->m_indices->size());

		platform::RenderingAPI const& api = Config::Get()->GetRenderingAPI();
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

		// Allocate our vector to ensure it's the correct size:
		meshData->m_UV0->resize(meshData->m_positions->size(), vec2(0, 0));

		for (size_t i = 0; i < meshData->m_indices->size(); i += 3)
		{
			meshData->m_UV0->at(meshData->m_indices->at(i)) = TL;
			meshData->m_UV0->at(meshData->m_indices->at(i + 1)) = BL;
			meshData->m_UV0->at(meshData->m_indices->at(i + 2)) = BR;
		}
	}


	void VertexAttributeBuilder::RemoveTriangleIndexing(MeshData* meshData)
	{
		const size_t numVerts = meshData->m_indices->size(); // Assume triangle lists: 3 index entries per triangle
		vector<uint32_t> newIndices(numVerts);
		vector<vec3> newPositions(numVerts);
		vector<vec3> newNormals(numVerts);
		vector<vec2> newUVs(numVerts);
		vector<vec4> newTangents(numVerts);

		// Use our indices to unpack duplicated vertex attributes:
		for (size_t i = 0; i < numVerts; i++)
		{
			newIndices[i] = (uint32_t)i;
			newPositions[i] = meshData->m_positions->at(meshData->m_indices->at(i));
			newNormals[i] = meshData->m_normals->at(meshData->m_indices->at(i));
			newUVs[i] = meshData->m_UV0->at(meshData->m_indices->at(i));
			newTangents[i] = meshData->m_tangents->at(meshData->m_indices->at(i));
		}

		*meshData->m_indices = move(newIndices);
		*meshData->m_positions = move(newPositions);
		*meshData->m_normals = move(newNormals);
		*meshData->m_UV0 = move(newUVs);
		*meshData->m_tangents = move(newTangents);
	}


	void VertexAttributeBuilder::WeldUnindexedTriangles(MeshData* meshData)
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


	int VertexAttributeBuilder::GetNumFaces(const SMikkTSpaceContext* m_context)
	{
		MeshData* meshData = static_cast<MeshData*> (m_context->m_pUserData);
		
		SEAssert("Unexpected number of indexes. Expected an exact factor of 3", meshData->m_indices->size() % 3 == 0);

		return (int)meshData->m_indices->size() / 3;
	}

	int VertexAttributeBuilder::GetNumFaceVerts(const SMikkTSpaceContext* m_context, const int faceIdx)
	{
		MeshData* meshData = static_cast<MeshData*> (m_context->m_pUserData);

		SEAssert("Only triangular faces are currently supported", 
			meshData->m_meshParams->m_drawMode == re::MeshPrimitive::DrawMode::Triangles);
		
		return 3;
	}

	void VertexAttributeBuilder::GetPosition(
		const SMikkTSpaceContext* m_context, float* outpos, const int faceIdx, const int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		int index = GetVertexIndex(m_context, faceIdx, vertIdx);
		vec3 position = meshData->m_positions->at(index);

		outpos[0] = position.x;
		outpos[1] = position.y;
		outpos[2] = position.z;
	}


	void VertexAttributeBuilder::GetNormal(
		const SMikkTSpaceContext* m_context, float* outnormal, const int faceIdx, const int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		int index = GetVertexIndex(m_context, faceIdx, vertIdx);
		vec3 normal = meshData->m_normals->at(index);;

		outnormal[0] = normal.x;
		outnormal[1] = normal.y;
		outnormal[2] = normal.z;
	}


	void VertexAttributeBuilder::GetTexCoords(
		const SMikkTSpaceContext* m_context, float* outuv, const int faceIdx, const int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		auto const& index = GetVertexIndex(m_context, faceIdx, vertIdx);
		vec2 uv = meshData->m_UV0->at(index);

		outuv[0] = uv.x;
		outuv[1] = uv.y;
	}


	void VertexAttributeBuilder::SetTangentSpaceBasic(
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


	int VertexAttributeBuilder::GetVertexIndex(const SMikkTSpaceContext* m_context, int faceIdx, int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		int faceSize = GetNumFaceVerts(m_context, faceIdx); // Currently only 3 supported...
		int indicesIdx = (faceIdx * faceSize) + vertIdx;
		int index = meshData->m_indices->at(indicesIdx);

		return index;
	}
}
