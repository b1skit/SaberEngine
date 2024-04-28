// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "RenderManager.h"
#include "VertexStreamBuilder.h"

#include "mikktspace.c" // LNK2019 otherwise...
#include "weldmesh.h"
#include "weldmesh.c" // LNK2019 otherwise...


namespace util
{
	void VertexStreamBuilder::BuildMissingVertexAttributes(MeshData* meshData)
	{
		util::VertexStreamBuilder tangentBuilder;
		tangentBuilder.ConstructMissingVertexAttributes(meshData);
	}


	VertexStreamBuilder::VertexStreamBuilder()
		: m_canBuildNormals(false)
		, m_canBuildTangents(false)
		, m_canBuildUVs(false)
		, m_canBuildColors(false)
		, m_hasJoints(false)
		, m_hasWeights(false)
	{
		m_interface.m_getNumFaces			= GetNumFaces;
		m_interface.m_getNumVerticesOfFace  = GetNumFaceVerts;
		m_interface.m_getPosition			= GetPosition;
		m_interface.m_getNormal				= GetNormal;		
		m_interface.m_getTexCoord			= GetTexCoords;
		m_interface.m_setTSpaceBasic		= SetTangentSpaceBasic;

		m_context.m_pInterface = &m_interface;
	}


	void VertexStreamBuilder::ConstructMissingVertexAttributes(MeshData* meshData)
	{
		const bool isTriangleList = meshData->m_meshParams->m_topologyMode == gr::MeshPrimitive::TopologyMode::TriangleList;
		SEAssert(meshData->m_indices && meshData->m_positions && isTriangleList,
			"Only indexed triangle lists are (currently) supported");

		LOG("Processing mesh \"%s\" with %d vertices...", meshData->m_name.c_str(), meshData->m_positions->size());

		// If an attribute does not exist but can be built, pass a vector of size 0

		m_canBuildNormals = meshData->m_normals != nullptr;
		const bool hasNormals = m_canBuildNormals && !meshData->m_normals->empty();
		
		m_canBuildTangents = meshData->m_tangents != nullptr;
		bool hasTangents = m_canBuildTangents && !meshData->m_tangents->empty();

		m_canBuildUVs = meshData->m_UV0 != nullptr;
		const bool hasUVs = m_canBuildUVs && !meshData->m_UV0->empty();

		m_canBuildColors = meshData->m_colors != nullptr;
		const bool hasColors = m_canBuildColors && !meshData->m_colors->empty();

		m_hasJoints = meshData->m_joints && !meshData->m_joints->empty();
		m_hasWeights = meshData->m_weights && !meshData->m_weights->empty();

		// Ensure we have the mandatory minimum vertex attributes:
		if (hasNormals && hasTangents && hasUVs && hasColors) // joints, weights are optional
		{
			LOG("Mesh \"%s\" has all required attributes", meshData->m_name.c_str());
			return; // Note: We skip degenerate triangle removal this way, but low risk as the asset came with all attribs
		}

		// Ensure that any valid indexes will not go out of bounds: Allocate enough space for any missing attributes:
		const size_t maxElements = std::max(meshData->m_indices->size(), 
			std::max(meshData->m_positions->size(), 
				std::max(m_canBuildNormals ? meshData->m_normals->size() : 0,
					std::max(m_canBuildTangents ? meshData->m_tangents->size() : 0,
						std::max(m_canBuildUVs ? meshData->m_UV0->size() : 0, 
							m_canBuildColors ? meshData->m_colors->size() : 0)))));

		if (!hasNormals && m_canBuildNormals)
		{
			meshData->m_normals->resize(maxElements, glm::vec3(0, 0, 0));

			if (hasTangents)
			{
				// GLTF 2.0 specs: When normals are not specified, client implementations MUST calculate flat normals 
				// and the provided tangents(if present) MUST be ignored.
				// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview
				meshData->m_tangents->clear();
				meshData->m_tangents->resize(meshData->m_indices->size(), glm::vec4(0, 0, 0, 0));
				hasTangents = false;
			}
		}
		if (!hasTangents && m_canBuildTangents)
		{
			meshData->m_tangents->resize(maxElements, glm::vec4(0, 0, 0, 0));
		}
		if (!hasUVs && m_canBuildUVs)
		{
			meshData->m_UV0->resize(maxElements, glm::vec2(0, 0));
		}
		if (!hasColors && m_canBuildColors)
		{
			meshData->m_colors->resize(maxElements, meshData->m_vertexColor);
		}

		// Expand shared attributes into distinct entries
		const bool hasSharedAttributes = meshData->m_indices->size() > meshData->m_positions->size();
		if (hasSharedAttributes)
		{
			LOG("MeshPrimitive \"%s\" contains shared vertex attributes, splitting...", meshData->m_name.c_str());
			SplitSharedAttributes(meshData);
		}

		// Find and remove any degenerate triangles:
		RemoveDegenerateTriangles(meshData);

		// Build any missing attributes:
		if (!hasNormals && m_canBuildNormals)
		{
			BuildFlatNormals(meshData);
		}
		if (!hasTangents && m_canBuildTangents)
		{
			LOG("MeshPrimitive \"%s\" is missing tangents, they will be generated...", meshData->m_name.c_str());

			m_context.m_pUserData = meshData;
			tbool result = genTangSpaceDefault(&this->m_context);
			SEAssert(result, "Failed to generate tangents");
		}
		if (!hasUVs && m_canBuildUVs)
		{
			BuildSimpleTriangleUVs(meshData);
		}

		// Reuse duplicate attributes, if required:
		if (hasSharedAttributes)
		{
			WeldTriangles(meshData);
		}

		LOG("Processed MeshPrimitive \"%s\" now has %d unique vertices",
			meshData->m_name.c_str(), meshData->m_positions->size());
	}


	void VertexStreamBuilder::RemoveDegenerateTriangles(MeshData* meshData)
	{
		SEAssert(meshData->m_indices->size() % 3 == 0, "Expected a triangle list");
		SEAssert(meshData->m_positions->size() >= meshData->m_indices->size(), "Expected a triangle list");
		SEAssert(!m_canBuildNormals || meshData->m_normals->size() >= meshData->m_indices->size(), "Expected a triangle list");
		SEAssert(!m_canBuildTangents || meshData->m_tangents->size() >= meshData->m_indices->size(), "Expected a triangle list");
		SEAssert(!m_canBuildUVs || meshData->m_UV0->size() >= meshData->m_indices->size(), "Expected a triangle list");
		SEAssert(!m_canBuildColors || meshData->m_colors->size() >= meshData->m_indices->size(), "Expected a triangle list");
		SEAssert((!m_hasJoints || meshData->m_joints->size() >= meshData->m_indices->size()), "Expected a triangle list");
		SEAssert((!m_hasWeights || meshData->m_weights->size() >= meshData->m_indices->size()), "Expected a triangle list");

		std::vector<uint32_t> newIndices;
		std::vector<glm::vec3> newPositions;
		std::vector<glm::vec3> newNormals;
		std::vector<glm::vec4> newTangents;
		std::vector<glm::vec2> newUVs;
		std::vector<glm::vec4> newColors;
		std::vector<glm::tvec4<uint8_t>> newJoints;
		std::vector<glm::vec4> newWeights;

		// We might remove verts, so reserve rather than resize...
		const size_t maxNumVerts = meshData->m_indices->size(); // Assume triangle lists: 3 index entries per triangle
		newIndices.reserve(maxNumVerts);
		newPositions.reserve(maxNumVerts);
		newNormals.reserve(maxNumVerts);
		newTangents.reserve(maxNumVerts);
		newUVs.reserve(maxNumVerts);
		newColors.reserve(maxNumVerts);
		if (m_hasJoints)
		{
			newJoints.reserve(maxNumVerts);
		}
		if (m_hasWeights)
		{
			newWeights.reserve(maxNumVerts);
		}

		size_t numDegeneratesFound = 0;
		uint32_t insertIdx = 0;
		for (size_t i = 0; i < meshData->m_indices->size(); i += 3)
		{
			const glm::vec3& p0 = meshData->m_positions->at(meshData->m_indices->at(i));
			const glm::vec3& p1 = meshData->m_positions->at(meshData->m_indices->at(i + 1));
			const glm::vec3& p2 = meshData->m_positions->at(meshData->m_indices->at(i + 2));

			const glm::vec3 v0 = p0 - p2;
			const glm::vec3 v1 = p1 - p2;
			const glm::vec3 v2 = p0 - p1;

			const float v0Length = glm::length(v0);
			const float v1Length = glm::length(v1);
			const float v2Length = glm::length(v2);

			const bool isValid =
				v0Length + v1Length > v2Length &&
				v0Length + v2Length > v1Length &&
				v1Length + v2Length > v0Length;

			if (isValid)
			{
				SEAssert(insertIdx == newPositions.size(), "Insertions are out of sync");
				newIndices.emplace_back(insertIdx++);
				newIndices.emplace_back(insertIdx++);
				newIndices.emplace_back(insertIdx++);

				newPositions.emplace_back(meshData->m_positions->at(meshData->m_indices->at(i)));
				newPositions.emplace_back(meshData->m_positions->at(meshData->m_indices->at(i + 1)));
				newPositions.emplace_back(meshData->m_positions->at(meshData->m_indices->at(i + 2)));

				if (m_canBuildNormals)
				{
					newNormals.emplace_back(meshData->m_normals->at(meshData->m_indices->at(i)));
					newNormals.emplace_back(meshData->m_normals->at(meshData->m_indices->at(i + 1)));
					newNormals.emplace_back(meshData->m_normals->at(meshData->m_indices->at(i + 2)));
				}

				if (m_canBuildTangents)
				{
					newTangents.emplace_back(meshData->m_tangents->at(meshData->m_indices->at(i)));
					newTangents.emplace_back(meshData->m_tangents->at(meshData->m_indices->at(i + 1)));
					newTangents.emplace_back(meshData->m_tangents->at(meshData->m_indices->at(i + 2)));
				}

				if (m_canBuildUVs)
				{
					newUVs.emplace_back(meshData->m_UV0->at(meshData->m_indices->at(i)));
					newUVs.emplace_back(meshData->m_UV0->at(meshData->m_indices->at(i + 1)));
					newUVs.emplace_back(meshData->m_UV0->at(meshData->m_indices->at(i + 2)));
				}

				if (m_canBuildColors)
				{
					newColors.emplace_back(meshData->m_colors->at(meshData->m_indices->at(i)));
					newColors.emplace_back(meshData->m_colors->at(meshData->m_indices->at(i + 1)));
					newColors.emplace_back(meshData->m_colors->at(meshData->m_indices->at(i + 2)));
				}

				if (m_hasJoints)
				{
					newJoints.emplace_back(meshData->m_joints->at(meshData->m_indices->at(i)));
					newJoints.emplace_back(meshData->m_joints->at(meshData->m_indices->at(i + 1)));
					newJoints.emplace_back(meshData->m_joints->at(meshData->m_indices->at(i + 2)));
				}
				if (m_hasWeights)
				{
					newWeights.emplace_back(meshData->m_weights->at(meshData->m_indices->at(i)));
					newWeights.emplace_back(meshData->m_weights->at(meshData->m_indices->at(i + 1)));
					newWeights.emplace_back(meshData->m_weights->at(meshData->m_indices->at(i + 2)));
				}
			}
			else
			{
				numDegeneratesFound++;
			}
		}

		*meshData->m_indices = move(newIndices);
		*meshData->m_positions = move(newPositions);

		if (m_canBuildNormals)
		{
			*meshData->m_normals = move(newNormals);
		}
		if (m_canBuildTangents)
		{
			*meshData->m_tangents = move(newTangents);
		}
		if (m_canBuildUVs)
		{
			*meshData->m_UV0 = move(newUVs);
		}
		if (m_canBuildColors)
		{
			*meshData->m_colors = move(newColors);
		}

		if (m_hasJoints)
		{
			*meshData->m_joints = move(newJoints);
		}
		if (m_hasWeights)
		{
			*meshData->m_weights = move(newWeights);
		}

		if (numDegeneratesFound > 0)
		{
			LOG_WARNING("Removed %d degenerate triangles from mesh \"%s\"", numDegeneratesFound, meshData->m_name.c_str());
		}
	}


	void VertexStreamBuilder::BuildFlatNormals(MeshData* meshData)
	{
		SEAssert(m_canBuildNormals && 
			meshData->m_indices->size() % 3 == 0 && 
			meshData->m_normals->size() == meshData->m_indices->size(),
			"Expected a triangle list and pre-allocated normals vector");

		LOG("MeshPrimitive \"%s\" is missing normals, generating flat normals...", meshData->m_name.c_str());

		for (size_t i = 0; i < meshData->m_indices->size(); i += 3)
		{
			const glm::vec3& p0 = meshData->m_positions->at(meshData->m_indices->at(i));
			const glm::vec3& p1 = meshData->m_positions->at(meshData->m_indices->at(i + 1));
			const glm::vec3& p2 = meshData->m_positions->at(meshData->m_indices->at(i + 2));

			const glm::vec3 v0 = p0 - p2;
			const glm::vec3 v1 = p1 - p2;

			const glm::vec3 faceNormal = glm::normalize(glm::cross(v0, v1));
			
			meshData->m_normals->at(meshData->m_indices->at(i)) = faceNormal;
			meshData->m_normals->at(meshData->m_indices->at(i + 1)) = faceNormal;
			meshData->m_normals->at(meshData->m_indices->at(i + 2)) = faceNormal;
		}
	}


	void VertexStreamBuilder::BuildSimpleTriangleUVs(MeshData* meshData)
	{
		SEAssert(m_canBuildUVs && 
			meshData->m_indices->size() % 3 == 0 && 
			meshData->m_UV0->size() == meshData->m_indices->size(),
			"Expected a triangle list and pre-allocated UV0 vector");

		LOG("MeshPrimitive \"%s\" is missing UVs, generating a simple set...", meshData->m_name.c_str());

		platform::RenderingAPI const& api = re::RenderManager::Get()->GetRenderingAPI();
		const bool botLeftZeroZero = api == platform::RenderingAPI::OpenGL ? true : false;

		// Build simple, overlapping UVs, placing the vertices of every triangle in the TL, BL, BR corners of UV space:
		glm::vec2 TL, BL, BR;
		if (botLeftZeroZero) // OpenGL-style: (0,0) in the bottom-left of UV space
		{
			TL = glm::vec2(0, 1);
			BL = glm::vec2(0, 0);
			BR = glm::vec2(1, 0);

		}
		else // D3D-style: (0,0) in the top-left of UV space
		{
			TL = glm::vec2(0, 0);
			BL = glm::vec2(0, 1);
			BR = glm::vec2(1, 1);
		}

		// Allocate our vector to ensure it's the correct size:
		meshData->m_UV0->resize(meshData->m_positions->size(), glm::vec2(0, 0));

		for (size_t i = 0; i < meshData->m_indices->size(); i += 3)
		{
			meshData->m_UV0->at(meshData->m_indices->at(i)) = TL;
			meshData->m_UV0->at(meshData->m_indices->at(i + 1)) = BL;
			meshData->m_UV0->at(meshData->m_indices->at(i + 2)) = BR;
		}
	}


	void VertexStreamBuilder::SplitSharedAttributes(MeshData* meshData)
	{
		const size_t numVerts = meshData->m_indices->size(); // Assume triangle lists: 3 index entries per triangle
		std::vector<uint32_t> newIndices(numVerts);
		std::vector<glm::vec3> newPositions(numVerts);
		std::vector<glm::vec3> newNormals(numVerts);
		std::vector<glm::vec4> newTangents(numVerts);
		std::vector<glm::vec2> newUVs(numVerts);
		std::vector<glm::vec4> newColors(numVerts);
		std::vector<glm::tvec4<uint8_t>> newJoints(m_hasJoints ? numVerts : 0);
		std::vector<glm::vec4> newWeights(m_hasWeights ? numVerts : 0);

		// Use our indices to unpack duplicated vertex attributes:
		for (size_t i = 0; i < numVerts; i++)
		{
			newIndices[i] = static_cast<uint32_t>(i);
			newPositions[i] = meshData->m_positions->at(meshData->m_indices->at(i));

			if (m_canBuildNormals)
			{
				newNormals[i] = meshData->m_normals->at(meshData->m_indices->at(i));
			}
			if (m_canBuildTangents)
			{
				newTangents[i] = meshData->m_tangents->at(meshData->m_indices->at(i));
			}
			if (m_canBuildUVs)
			{
				newUVs[i] = meshData->m_UV0->at(meshData->m_indices->at(i));
			}
			if (m_canBuildColors)
			{
				newColors[i] = meshData->m_colors->at(meshData->m_indices->at(i));
			}

			if (m_hasJoints)
			{
				newJoints[i] = meshData->m_joints->at(meshData->m_indices->at(i));
			}
			if (m_hasWeights)
			{
				newWeights[i] = meshData->m_weights->at(meshData->m_indices->at(i));
			}
		}

		*meshData->m_indices	= move(newIndices);
		*meshData->m_positions	= move(newPositions);

		if (m_canBuildNormals)
		{
			*meshData->m_normals = move(newNormals);
		}
		if (m_canBuildTangents)
		{
			*meshData->m_tangents = move(newTangents);
		}
		if (m_canBuildUVs)
		{
			*meshData->m_UV0 = move(newUVs);
		}
		if (m_canBuildColors)
		{
			*meshData->m_colors = move(newColors);
		}

		if (m_hasJoints)
		{
			*meshData->m_joints = move(newJoints);
		}
		if (m_hasWeights)
		{
			*meshData->m_weights = move(newWeights);
		}
	}


	void VertexStreamBuilder::WeldTriangles(MeshData* meshData)
	{
		SEAssert(sizeof(int) == sizeof(uint32_t),
			"Mikktspace operates on system's int, SaberEngine operates on explicit 32-bit uints");

		LOG("Re-welding %d vertices to build unique vertex index list for mesh \"%s\"",
			meshData->m_positions->size(), meshData->m_name.c_str());

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
		std::vector<int> remapTable(meshData->m_positions->size(), 0); // This will contain our final indexes

		// We'll pack our vertex attributes together into blocks of floats.
		// Compute the total number of floats for all attributes per vertex:
		const size_t floatsPerVertex = (
			sizeof(glm::vec3)										// position
			+ (m_canBuildNormals ? sizeof(glm::vec3) : 0)			// normal
			+ (m_canBuildTangents ? sizeof(glm::vec4) : 0)			// tangent
			+ (m_canBuildUVs ? sizeof(glm::vec2) : 0)				// uv0
			+ (m_canBuildColors ? sizeof(glm::vec4) : 0)			// color
			+ (m_hasJoints ? sizeof(glm::tvec4<uint8_t>) : 0)		// joints
			+ (m_hasWeights ? sizeof(glm::vec4) : 0)				// weights
				) / sizeof(float);
		
		// Make sure we've counted for all non-index members in MeshData
		SEAssert(floatsPerVertex == (3 + 
				m_canBuildNormals * 3 +
				m_canBuildTangents * 4 +
				m_canBuildUVs * 2 +
				m_canBuildColors * 4 +
				m_hasJoints * 1 + 
				m_hasWeights * 4),
			"Data size mismatch/miscalulation");

		// pfVertexDataOut: iNrVerticesIn * iFloatsPerVert * sizeof(float)
		const size_t numElements = meshData->m_positions->size();
		SEAssert(meshData->m_positions->size() == meshData->m_indices->size(), "Unexpected position/index size mismatch");

		const size_t vertexStrideBytes = floatsPerVertex * sizeof(float);
		const size_t numVertexBytesOut = numElements * vertexStrideBytes;
		std::vector<float> vertexDataOut(numVertexBytesOut, 0); // Will contain only unique vertices after welding

		// pfVertexDataIn: Our tightly-packed vertex data:
		std::vector<float> packedVertexData(meshData->m_positions->size() * floatsPerVertex, 0);		

		size_t byteOffset = 0;
		PackAttribute(
			(float*)meshData->m_positions->data(), 
			packedVertexData.data(), 
			byteOffset,
			vertexStrideBytes,
			numElements,
			sizeof(glm::vec3));	// Position = glm::vec3
		byteOffset += sizeof(glm::vec3);
		
		if (m_canBuildNormals)
		{
			PackAttribute(
				(float*)meshData->m_normals->data(),
				packedVertexData.data(),
				byteOffset,
				vertexStrideBytes,
				numElements,
				sizeof(glm::vec3));	// Normals = glm::vec3
			byteOffset += sizeof(glm::vec3);
		}

		if (m_canBuildTangents)
		{
			PackAttribute(
				(float*)meshData->m_tangents->data(),
				packedVertexData.data(),
				byteOffset,
				vertexStrideBytes,
				numElements,
				sizeof(glm::vec4));	// tangents = glm::vec4
			byteOffset += sizeof(glm::vec4);
		}

		if (m_canBuildUVs)
		{
			PackAttribute(
				(float*)meshData->m_UV0->data(),
				packedVertexData.data(),
				byteOffset,
				vertexStrideBytes,
				numElements,
				sizeof(glm::vec2));	// UV0 = glm::vec2
			byteOffset += sizeof(glm::vec2);
		}

		if (m_canBuildColors)
		{
			PackAttribute(
				(float*)meshData->m_colors->data(),
				packedVertexData.data(),
				byteOffset,
				vertexStrideBytes,
				numElements,
				sizeof(glm::vec4));	// colors = glm::vec4
			byteOffset += sizeof(glm::vec4);
		}

		if (m_hasJoints)
		{
			PackAttribute(
				(uint8_t*)meshData->m_joints->data(),
				packedVertexData.data(),
				byteOffset,
				vertexStrideBytes,
				numElements,
				sizeof(glm::tvec4<uint8_t>));	// joints = tvec4<uint8_t>
			byteOffset += sizeof(glm::tvec4<uint8_t>);
		}
		if (m_hasWeights)
		{
			PackAttribute(
				(float*)meshData->m_weights->data(),
				packedVertexData.data(),
				byteOffset,
				vertexStrideBytes,
				numElements,
				sizeof(glm::vec4));	// weights = glm::vec4
			byteOffset += sizeof(glm::vec4);
		}


		// Weld the verts to obtain our final unique indexing:
		const int numUniqueVertsFound = WeldMesh(
			remapTable.data(), vertexDataOut.data(), packedVertexData.data(), (int)numElements, (int)floatsPerVertex);

		// Repack existing data streams according to the updated indexes:
		meshData->m_indices->resize(remapTable.size());
		meshData->m_positions->resize(numUniqueVertsFound);

		if (m_canBuildNormals)
		{
			meshData->m_normals->resize(numUniqueVertsFound);
		}
		if (m_canBuildTangents)
		{
			meshData->m_tangents->resize(numUniqueVertsFound);
		}
		if (m_canBuildUVs)
		{
			meshData->m_UV0->resize(numUniqueVertsFound);
		}
		if (m_canBuildColors)
		{
			meshData->m_colors->resize(numUniqueVertsFound);
		}
		if (m_hasJoints)
		{
			meshData->m_joints->resize(numUniqueVertsFound);
		}
		if (m_hasWeights)
		{
			meshData->m_weights->resize(numUniqueVertsFound);
		}

		for (size_t i = 0; i < remapTable.size(); i++)
		{
			const int vertexIndex = remapTable[i];
			meshData->m_indices->at(i) = (uint32_t)vertexIndex;

			// Pointer to the first byte in our blob of interleaved vertex data:
			const uint8_t* currentVertStart = (uint8_t*)vertexDataOut.data() + ((size_t)vertexIndex * vertexStrideBytes);

			// Copy each element back into its individual data stream:
			uint32_t packedVertByteOffset = 0;
			memcpy(&meshData->m_positions->at(vertexIndex).x, currentVertStart + packedVertByteOffset, sizeof(glm::vec3));
			packedVertByteOffset += sizeof(glm::vec3);

			if (m_canBuildNormals)
			{
				memcpy(&meshData->m_normals->at(vertexIndex).x, currentVertStart + packedVertByteOffset, sizeof(glm::vec3));
				packedVertByteOffset += sizeof(glm::vec3);
			}
			if (m_canBuildTangents)
			{
				memcpy(&meshData->m_tangents->at(vertexIndex).x, currentVertStart + packedVertByteOffset, sizeof(glm::vec4));
				packedVertByteOffset += sizeof(glm::vec4);
			}
			if (m_canBuildUVs)
			{
				memcpy(&meshData->m_UV0->at(vertexIndex).x, currentVertStart + packedVertByteOffset, sizeof(glm::vec2));
				packedVertByteOffset += sizeof(glm::vec2);
			}
			if (m_canBuildColors)
			{
				memcpy(&meshData->m_colors->at(vertexIndex).x, currentVertStart + packedVertByteOffset, sizeof(glm::vec4));
				packedVertByteOffset += sizeof(glm::vec4);
			}

			if (m_hasJoints)
			{
				memcpy(&meshData->m_joints->at(vertexIndex).x, currentVertStart + packedVertByteOffset, sizeof(glm::tvec4<uint8_t>));
				packedVertByteOffset += sizeof(glm::tvec4<uint8_t>);
			}
			if (m_hasWeights)
			{
				memcpy(&meshData->m_weights->at(vertexIndex).x, currentVertStart + packedVertByteOffset, sizeof(glm::vec4));
				packedVertByteOffset += sizeof(glm::vec4);
			}
		}
	}


	int VertexStreamBuilder::GetNumFaces(const SMikkTSpaceContext* m_context)
	{
		MeshData* meshData = static_cast<MeshData*> (m_context->m_pUserData);
		
		SEAssert(meshData->m_indices->size() % 3 == 0, "Unexpected number of indexes. Expected an exact factor of 3");

		return (int)meshData->m_indices->size() / 3;
	}


	int VertexStreamBuilder::GetNumFaceVerts(const SMikkTSpaceContext* m_context, const int faceIdx)
	{
		MeshData* meshData = static_cast<MeshData*> (m_context->m_pUserData);

		SEAssert(meshData->m_meshParams->m_topologyMode == gr::MeshPrimitive::TopologyMode::TriangleList,
			"Only triangular faces are currently supported");
		
		return 3;
	}


	void VertexStreamBuilder::GetPosition(
		const SMikkTSpaceContext* m_context, float* outpos, const int faceIdx, const int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		int index = GetVertexIndex(m_context, faceIdx, vertIdx);
		glm::vec3 position = meshData->m_positions->at(index);

		outpos[0] = position.x;
		outpos[1] = position.y;
		outpos[2] = position.z;
	}


	void VertexStreamBuilder::GetNormal(
		const SMikkTSpaceContext* m_context, float* outnormal, const int faceIdx, const int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		int index = GetVertexIndex(m_context, faceIdx, vertIdx);
		glm::vec3 normal = meshData->m_normals->at(index);;

		outnormal[0] = normal.x;
		outnormal[1] = normal.y;
		outnormal[2] = normal.z;
	}


	void VertexStreamBuilder::GetTexCoords(
		const SMikkTSpaceContext* m_context, float* outuv, const int faceIdx, const int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		auto const& index = GetVertexIndex(m_context, faceIdx, vertIdx);
		glm::vec2 uv = meshData->m_UV0->at(index);

		outuv[0] = uv.x;
		outuv[1] = uv.y;
	}


	void VertexStreamBuilder::SetTangentSpaceBasic(
		const SMikkTSpaceContext* m_context, const float* tangentu, const float fSign, const int faceIdx, const int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		int index = GetVertexIndex(m_context, faceIdx, vertIdx);
		glm::vec4* tangent = &meshData->m_tangents->at(index);

		tangent->x = tangentu[0];
		tangent->y = tangentu[1];
		tangent->z = tangentu[2];

#if defined(UPPER_LEFT_UV_ORIGIN)
		tangent->w = -fSign;
#else
		tangent->w = fSign;
#endif
	}


	int VertexStreamBuilder::GetVertexIndex(const SMikkTSpaceContext* m_context, int faceIdx, int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		int faceSize = GetNumFaceVerts(m_context, faceIdx); // Currently only 3 supported...
		int indicesIdx = (faceIdx * faceSize) + vertIdx;
		int index = meshData->m_indices->at(indicesIdx);

		return index;
	}
}
