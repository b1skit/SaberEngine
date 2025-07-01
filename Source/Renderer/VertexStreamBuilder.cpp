// © 2022 Adam Badke. All rights reserved.
#include "VertexStreamBuilder.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/Logger.h"

#include "mikktspace.c" // LNK2019 otherwise...
#include "weldmesh.h"
#include "weldmesh.c" // LNK2019 otherwise...


namespace grutil
{
	void VertexStreamBuilder::BuildMissingVertexAttributes(MeshData* meshData)
	{
		grutil::VertexStreamBuilder tangentBuilder;
		tangentBuilder.ConstructMissingVertexAttributes(meshData);
	}


	VertexStreamBuilder::VertexStreamBuilder()
		: m_canBuildNormals(false)
		, m_canBuildTangents(false)
		, m_canBuildUVs(false)
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
		const bool isTriangleList = 
			meshData->m_meshParams->m_primitiveTopology == gr::MeshPrimitive::PrimitiveTopology::TriangleList;
		SEAssert(meshData->m_indices && meshData->m_positions && isTriangleList,
			"Only triangle lists are (currently) supported");

		LOG("Processing mesh \"%s\" with %d vertices...", meshData->m_name.c_str(), meshData->m_positions->size());

		if (meshData->m_indices->empty())
		{
			BuildIndexList(meshData);
		}

		m_canBuildNormals = meshData->m_normals != nullptr;
		const bool hasNormals = m_canBuildNormals && !meshData->m_normals->empty();
		
		m_canBuildTangents = meshData->m_tangents != nullptr;
		bool hasTangents = m_canBuildTangents && !meshData->m_tangents->empty();

		m_canBuildUVs = meshData->m_UV0 != nullptr;
		const bool hasUVs = m_canBuildUVs && !meshData->m_UV0->empty();

		// Ensure we have the mandatory minimum vertex attributes:
		if (hasNormals && hasTangents && hasUVs)
		{
			LOG("Mesh \"%s\" has all required attributes", meshData->m_name.c_str());
			return; // Note: We skip degenerate triangle removal this way, but low risk as the asset came with all attribs
		}

		// Ensure that any valid indexes will not go out of bounds: Allocate enough space for any missing attributes:
		const size_t maxElements = meshData->m_indices->size();

		if (!hasNormals && m_canBuildNormals)
		{
			meshData->m_normals->resize(maxElements, glm::vec3(0.f));

			if (hasTangents)
			{
				// GLTF 2.0 specs: When normals are not specified, client implementations MUST calculate flat normals 
				// and the provided tangents(if present) MUST be ignored.
				// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview
				meshData->m_tangents->clear();
				hasTangents = false;
			}
		}
		if (!hasTangents && m_canBuildTangents)
		{
			meshData->m_tangents->resize(maxElements, glm::vec4(0.f));
		}
		if (!hasUVs && m_canBuildUVs)
		{
			meshData->m_UV0->resize(maxElements, glm::vec2(0.f));
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

		if (!hasUVs && m_canBuildUVs)
		{
			BuildSimpleTriangleUVs(meshData);
		}

		if (!hasTangents && m_canBuildTangents)
		{
			LOG("MeshPrimitive \"%s\" is missing tangents, they will be generated...", meshData->m_name.c_str());

			m_context.m_pUserData = meshData;
			const tbool result = genTangSpaceDefault(&this->m_context);
			SEAssert(result, "Failed to generate tangents");
		}

		// Reuse duplicate attributes, if required:
		if (hasSharedAttributes)
		{
			WeldTriangles(meshData);
		}

		LOG("Processed MeshPrimitive \"%s\" now has %d unique vertices",
			meshData->m_name.c_str(), meshData->m_positions->size());
	}


	void VertexStreamBuilder::SplitSharedAttributes(MeshData* meshData)
	{
		SEAssert(meshData->m_indices->IsScalarType<uint16_t>() || meshData->m_indices->IsScalarType<uint32_t>(),
			"Unexpected index format");

		const size_t numIndices = meshData->m_indices->size(); // Assume triangle lists: 3 index entries per triangle

		// Shrink our indices to 16 bits if possible:
		const bool use16BitIndices = numIndices < std::numeric_limits<uint16_t>::max();

		util::ByteVector newIndices = use16BitIndices ?
			util::ByteVector::Create<uint16_t>(numIndices) : util::ByteVector::Create<uint32_t>(numIndices);

		SEAssert(meshData->m_indicesStreamDesc || 
			newIndices.IsScalarType<uint16_t>() == meshData->m_indices->IsScalarType<uint16_t>(),
			"Indices stream ptr is null, yet we must change the indices data type");

		if (meshData->m_indicesStreamDesc &&
			newIndices.IsScalarType<uint16_t>() != meshData->m_indices->IsScalarType<uint16_t>())
		{
			if (use16BitIndices)
			{
				meshData->m_indicesStreamDesc->m_dataType = re::DataType::UShort;
			}
			else
			{
				meshData->m_indicesStreamDesc->m_dataType = re::DataType::UInt;
			}
		}

		util::ByteVector newPositions = util::ByteVector::Create<glm::vec3>(numIndices);

		util::ByteVector newNormals = util::ByteVector::Create<glm::vec3>();
		if (m_canBuildNormals)
		{
			newNormals.resize(numIndices);
		}

		util::ByteVector newTangents = util::ByteVector::Create<glm::vec4>();
		if (m_canBuildTangents)
		{
			newTangents.resize(numIndices);
		}

		util::ByteVector newUVs = util::ByteVector::Create<glm::vec2>();
		if (m_canBuildUVs)
		{
			newUVs.resize(numIndices);
		}

		std::vector<util::ByteVector> newExtraChannels;
		newExtraChannels.reserve(meshData->m_extraChannels->size());
		for (size_t i = 0; i < meshData->m_extraChannels->size(); ++i)
		{
			newExtraChannels.emplace_back(
				util::ByteVector::Clone(*meshData->m_extraChannels->at(i), util::ByteVector::CloneMode::Empty));
			newExtraChannels.back().resize(numIndices);
		}

		// Use our indices to unpack duplicated vertex attributes:
		for (size_t dstIdx = 0; dstIdx < numIndices; dstIdx++)
		{
			const uint32_t srcIdx = meshData->m_indices->ScalarGetAs<uint32_t>(dstIdx);

			newIndices.ScalarSetFrom<uint32_t>(dstIdx, static_cast<uint32_t>(dstIdx));

			newPositions.at<glm::vec3>(dstIdx) = meshData->m_positions->at<glm::vec3>(srcIdx);

			if (m_canBuildNormals)
			{
				newNormals.at<glm::vec3>(dstIdx) = meshData->m_normals->at<glm::vec3>(srcIdx);
			}
			if (m_canBuildTangents)
			{
				newTangents.at<glm::vec4>(dstIdx) = meshData->m_tangents->at<glm::vec4>(srcIdx);
			}
			if (m_canBuildUVs)
			{
				newUVs.at<glm::vec2>(dstIdx) = meshData->m_UV0->at<glm::vec2>(srcIdx);
			}

			for (size_t i = 0; i < meshData->m_extraChannels->size(); ++i)
			{
				util::ByteVector::CopyElement(newExtraChannels[i], dstIdx, *meshData->m_extraChannels->at(i), srcIdx);
			}
		}

		*meshData->m_indices = std::move(newIndices);
		*meshData->m_positions = std::move(newPositions);

		if (m_canBuildNormals)
		{
			*meshData->m_normals = std::move(newNormals);
		}
		if (m_canBuildTangents)
		{
			*meshData->m_tangents = std::move(newTangents);
		}
		if (m_canBuildUVs)
		{
			*meshData->m_UV0 = std::move(newUVs);
		}

		for (size_t i = 0; i < meshData->m_extraChannels->size(); ++i)
		{
			*meshData->m_extraChannels->at(i) = std::move(newExtraChannels[i]);
		}
	}


	void VertexStreamBuilder::RemoveDegenerateTriangles(MeshData* meshData)
	{
		SEAssert(meshData->m_indices->size() % 3 == 0, "Expected a triangle list");
		SEAssert(meshData->m_positions->size() >= meshData->m_indices->size(), "Expected a triangle list");
		SEAssert(!m_canBuildNormals || meshData->m_normals->size() >= meshData->m_indices->size(), "Expected a triangle list");
		SEAssert(!m_canBuildTangents || meshData->m_tangents->size() >= meshData->m_indices->size(), "Expected a triangle list");
		SEAssert(!m_canBuildUVs || meshData->m_UV0->size() >= meshData->m_indices->size(), "Expected a triangle list");

		auto ValidateVertex = [](util::ByteVector const* indices, util::ByteVector const* positions, size_t firstIdx)
			-> bool
			{
				glm::vec3 const& p0 = positions->at<glm::vec3>(indices->ScalarGetAs<uint32_t>(firstIdx));
				glm::vec3 const& p1 = positions->at<glm::vec3>(indices->ScalarGetAs<uint32_t>(firstIdx + 1));
				glm::vec3 const& p2 = positions->at<glm::vec3>(indices->ScalarGetAs<uint32_t>(firstIdx + 2));

				glm::vec3 const& v0 = p0 - p2;
				glm::vec3 const& v1 = p1 - p2;
				glm::vec3 const& v2 = p0 - p1;

				const float v0Length = glm::length(v0);
				const float v1Length = glm::length(v1);
				const float v2Length = glm::length(v2);

				const bool isValid =
					v0Length + v1Length > v2Length &&
					v0Length + v2Length > v1Length &&
					v1Length + v2Length > v0Length;

				return isValid;
			};

		// Pre-check the data; No need to rebuild anything if we don't detect any degenerate triangles. This is slightly
		// slower when degenerate triangles exist, but slightly faster when they don't
		bool isValid = true;
		for (size_t i = 0; i < meshData->m_indices->size(); i += 3)
		{
			isValid = ValidateVertex(meshData->m_indices, meshData->m_positions, i);
			if (!isValid)
			{
				break;
			}
		}
		if (isValid)
		{
			return;
		}


		// We might remove verts, so reserve rather than resize...
		const size_t maxNumVerts = meshData->m_indices->size(); // Assume triangle lists: 3 index entries per triangle

		SEAssert(meshData->m_indices->IsScalarType<uint16_t>() || meshData->m_indices->IsScalarType<uint32_t>(),
			"Unexpected index format");

		util::ByteVector newIndices = meshData->m_indices->IsScalarType<uint16_t>() ?
			util::ByteVector::Create<uint16_t>() :
			util::ByteVector::Create<uint32_t>();
		newIndices.reserve(maxNumVerts);
		util::ByteVector newPositions = util::ByteVector::Create<glm::vec3>();
		newPositions.reserve(maxNumVerts);
		util::ByteVector newNormals = util::ByteVector::Create<glm::vec3>();
		if (m_canBuildNormals)
		{
			newNormals.reserve(maxNumVerts);
		}
		util::ByteVector newTangents = util::ByteVector::Create<glm::vec4>();
		if (m_canBuildTangents)
		{
			newTangents.reserve(maxNumVerts);
		}
		util::ByteVector newUVs = util::ByteVector::Create<glm::vec2>();
		if (m_canBuildUVs)
		{
			newUVs.reserve(maxNumVerts);
		}

		std::vector<util::ByteVector> newExtraChannels;
		newExtraChannels.reserve(meshData->m_extraChannels->size());
		for (size_t i = 0; i < meshData->m_extraChannels->size(); ++i)
		{
			newExtraChannels.emplace_back(
				util::ByteVector::Clone(*meshData->m_extraChannels->at(i), util::ByteVector::CloneMode::Empty));
			newExtraChannels.back().reserve(maxNumVerts);
		}

		size_t numDegeneratesFound = 0;
		uint32_t insertIdx = 0;
		for (size_t i = 0; i < meshData->m_indices->size(); i += 3)
		{
			const bool vertIsValid = ValidateVertex(meshData->m_indices, meshData->m_positions, i);

			if (vertIsValid)
			{
				SEAssert(insertIdx == newPositions.size(), "Insertions are out of sync");

				if (meshData->m_indices->IsScalarType<uint16_t>())
				{
					newIndices.emplace_back<uint16_t>(insertIdx++);
					newIndices.emplace_back<uint16_t>(insertIdx++);
					newIndices.emplace_back<uint16_t>(insertIdx++);
				}
				else
				{
					newIndices.emplace_back<uint32_t>(insertIdx++);
					newIndices.emplace_back<uint32_t>(insertIdx++);
					newIndices.emplace_back<uint32_t>(insertIdx++);
				}

				const uint32_t curIndices_i = meshData->m_indices->ScalarGetAs<uint32_t>(i);
				const uint32_t curIndices_i1 = meshData->m_indices->ScalarGetAs<uint32_t>(i + 1);
				const uint32_t curIndices_i2 = meshData->m_indices->ScalarGetAs<uint32_t>(i + 2);

				newPositions.emplace_back(meshData->m_positions->at<glm::vec3>(curIndices_i));
				newPositions.emplace_back(meshData->m_positions->at<glm::vec3>(curIndices_i1));
				newPositions.emplace_back(meshData->m_positions->at<glm::vec3>(curIndices_i2));

				if (m_canBuildNormals)
				{
					newNormals.emplace_back(meshData->m_normals->at<glm::vec3>(curIndices_i));
					newNormals.emplace_back(meshData->m_normals->at<glm::vec3>(curIndices_i1));
					newNormals.emplace_back(meshData->m_normals->at<glm::vec3>(curIndices_i2));
				}

				if (m_canBuildTangents)
				{
					newTangents.emplace_back(meshData->m_tangents->at<glm::vec4>(curIndices_i));
					newTangents.emplace_back(meshData->m_tangents->at<glm::vec4>(curIndices_i1));
					newTangents.emplace_back(meshData->m_tangents->at<glm::vec4>(curIndices_i2));
				}

				if (m_canBuildUVs)
				{
					newUVs.emplace_back(meshData->m_UV0->at<glm::vec2>(curIndices_i));
					newUVs.emplace_back(meshData->m_UV0->at<glm::vec2>(curIndices_i1));
					newUVs.emplace_back(meshData->m_UV0->at<glm::vec2>(curIndices_i2));
				}

				for (size_t chanIdx = 0; chanIdx < meshData->m_extraChannels->size(); ++chanIdx)
				{
					util::ByteVector::EmplaceBackElement(
						newExtraChannels[chanIdx], *meshData->m_extraChannels->at(chanIdx), curIndices_i);
					util::ByteVector::EmplaceBackElement(
						newExtraChannels[chanIdx], *meshData->m_extraChannels->at(chanIdx), curIndices_i1);
					util::ByteVector::EmplaceBackElement(
						newExtraChannels[chanIdx], *meshData->m_extraChannels->at(chanIdx), curIndices_i2);
				}
			}
			else
			{
				numDegeneratesFound++;
			}
		}

		*meshData->m_indices = std::move(newIndices);
		*meshData->m_positions = std::move(newPositions);

		if (m_canBuildNormals)
		{
			*meshData->m_normals = std::move(newNormals);
		}
		if (m_canBuildTangents)
		{
			*meshData->m_tangents = std::move(newTangents);
		}
		if (m_canBuildUVs)
		{
			*meshData->m_UV0 = std::move(newUVs);
		}

		for (size_t i = 0; i < meshData->m_extraChannels->size(); ++i)
		{
			*meshData->m_extraChannels->at(i) = std::move(newExtraChannels[i]);
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
			const uint32_t i0 = meshData->m_indices->ScalarGetAs<uint32_t>(i);
			const uint32_t i1 = meshData->m_indices->ScalarGetAs<uint32_t>(i + 1);
			const uint32_t i2 = meshData->m_indices->ScalarGetAs<uint32_t>(i + 2);

			glm::vec3 const& p0 = meshData->m_positions->at<glm::vec3>(i0);
			glm::vec3 const& p1 = meshData->m_positions->at<glm::vec3>(i1);
			glm::vec3 const& p2 = meshData->m_positions->at<glm::vec3>(i2);

			glm::vec3 const& v0 = p0 - p2;
			glm::vec3 const& v1 = p1 - p2;

			glm::vec3 const& faceNormal = glm::normalize(glm::cross(v0, v1));
			
			meshData->m_normals->at<glm::vec3>(i0) = faceNormal;
			meshData->m_normals->at<glm::vec3>(i1) = faceNormal;
			meshData->m_normals->at<glm::vec3>(i2) = faceNormal;
		}
	}


	void VertexStreamBuilder::BuildIndexList(MeshData* meshData)
	{
		SEAssert(meshData->m_indices && meshData->m_indices->empty(), "Invalid configuration for building an index list");

		// Create a simple index list
		meshData->m_indices->resize(meshData->m_positions->size());
		for (uint32_t i = 0; i < meshData->m_positions->size(); ++i)
		{
			meshData->m_indices->ScalarSetFrom(i, i);
		}
	}


	void VertexStreamBuilder::BuildSimpleTriangleUVs(MeshData* meshData)
	{
		SEAssert(m_canBuildUVs && 
			meshData->m_indices->size() % 3 == 0 && 
			meshData->m_UV0->size() == meshData->m_indices->size(),
			"Expected a triangle list and pre-allocated TexCoord0 vector");

		SEAssert(meshData->m_UV0->size() == meshData->m_positions->size(), "Unexpected UV allocation size");

		LOG("MeshPrimitive \"%s\" is missing UVs, generating a simple set...", meshData->m_name.c_str());

		const platform::RenderingAPI api =
			core::Config::Get()->GetValue<platform::RenderingAPI>(core::configkeys::k_renderingAPIKey);
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

		for (size_t i = 0; i < meshData->m_indices->size(); i += 3)
		{
			meshData->m_UV0->at<glm::vec2>(meshData->m_indices->ScalarGetAs<uint32_t>(i)) = TL;
			meshData->m_UV0->at<glm::vec2>(meshData->m_indices->ScalarGetAs<uint32_t>(i + 1)) = BL;
			meshData->m_UV0->at<glm::vec2>(meshData->m_indices->ScalarGetAs<uint32_t>(i + 2)) = BR;
		}
	}


	void VertexStreamBuilder::WeldTriangles(MeshData* meshData)
	{
		SEStaticAssert(sizeof(int) == sizeof(uint32_t),
			"Mikktspace operates on system's int, SaberEngine operates on explicit 32-bit uints");

		SEAssert(meshData->m_positions->size() == meshData->m_indices->size() &&
			(!meshData->m_normals || meshData->m_normals->size() == meshData->m_indices->size()) &&
			(!meshData->m_tangents || meshData->m_tangents->size() == meshData->m_indices->size()) &&
			(!meshData->m_UV0 || meshData->m_UV0->size() == meshData->m_indices->size()),
			"Expecting streams should be the same size before welding");

		LOG("Re-welding %d vertices to build unique vertex index list for mesh \"%s\"",
			meshData->m_indices->size(), meshData->m_name.c_str());

		// The Mikktspace welder expects tightly-packed vertex data; Pack it to get the index list, then reorder our
		// individual streams once welding is complete
		auto PackAttribute = [](
			void* src, void* dest, size_t byteOffset, size_t strideBytes, size_t numElements, size_t elementBytes)
		{
			for (size_t i = 0; i < numElements; i++)
			{
				void* currentSrc = static_cast<uint8_t*>(src) + (elementBytes * i);
				void* currentDest = static_cast<uint8_t*>(dest) + byteOffset + (strideBytes * i);
				
				std::memcpy(currentDest, currentSrc, elementBytes);
			}
		};

		// piRemapTable: iNrVerticesIn * sizeof(int)
		std::vector<int> remapTable(meshData->m_indices->size(), 0); // This will contain our final indexes

		// We'll pack our vertex attributes together into blocks of floats.
		// Compute the total number of floats for all attributes per vertex:
		size_t bytesPerVertex = (
			sizeof(glm::vec3)										// position
			+ (m_canBuildNormals ? sizeof(glm::vec3) : 0)			// normal
			+ (m_canBuildTangents ? sizeof(glm::vec4) : 0)			// tangent
			+ (m_canBuildUVs ? sizeof(glm::vec2) : 0) );			// UV0

		for (auto const& extraChannel : *meshData->m_extraChannels)
		{
			bytesPerVertex += extraChannel->GetElementByteSize();
		}

		// pfVertexDataOut: iNrVerticesIn * iFloatsPerVert * sizeof(float)
		const size_t numElements = meshData->m_indices->size();

		const size_t vertexStrideBytes = bytesPerVertex;
		const size_t numVertexBytesOut = numElements * vertexStrideBytes;
		std::vector<float> vertexDataOut(numVertexBytesOut, 0); // Will contain only unique vertices after welding

		// pfVertexDataIn: Our tightly-packed vertex data:
		const size_t floatsPerVertex = (bytesPerVertex / sizeof(float));
		std::vector<float> packedVertexData(numElements * floatsPerVertex, 0);

		size_t byteOffset = 0;
		PackAttribute(
			meshData->m_positions->data().data(),
			packedVertexData.data(), 
			byteOffset,
			vertexStrideBytes,
			numElements,
			sizeof(glm::vec3));	// Position = glm::vec3
		byteOffset += sizeof(glm::vec3);
		
		if (m_canBuildNormals)
		{
			PackAttribute(
				meshData->m_normals->data().data(),
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
				meshData->m_tangents->data().data(),
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
				meshData->m_UV0->data().data(),
				packedVertexData.data(),
				byteOffset,
				vertexStrideBytes,
				numElements,
				sizeof(glm::vec2));	// TexCoord0 = glm::vec2
			byteOffset += sizeof(glm::vec2);
		}
		for (auto const& extraChannel : *meshData->m_extraChannels)
		{
			const uint8_t elementByteSize = extraChannel->GetElementByteSize();
			
			PackAttribute(
				extraChannel->data().data(),
				packedVertexData.data(),
				byteOffset,
				vertexStrideBytes,
				numElements,
				elementByteSize);
			byteOffset += elementByteSize;
		}


		// Weld the verts to obtain our final unique indexing:
		const int numUniqueVertsFound = WeldMesh(
			remapTable.data(),
			vertexDataOut.data(),
			packedVertexData.data(),
			static_cast<int>(numElements),
			static_cast<int>(floatsPerVertex));


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
		for (auto& extraChannel : *meshData->m_extraChannels)
		{
			extraChannel->resize(numUniqueVertsFound);
		}

		for (size_t i = 0; i < remapTable.size(); i++)
		{
			const uint32_t srcIdx = meshData->m_indices->ScalarGetAs<uint32_t>(i);

			const int vertIdx = remapTable[i];
			meshData->m_indices->ScalarSetFrom<uint32_t>(i, static_cast<uint32_t>(vertIdx));

			// Pointer to the first byte in our blob of interleaved vertex data:
			uint8_t const* currentVertStart = (uint8_t*)vertexDataOut.data() + ((size_t)vertIdx * vertexStrideBytes);

			// Copy each element back into its individual data stream:
			uint32_t packedVertByteOffset = 0;
			memcpy(&meshData->m_positions->at<glm::vec3>(vertIdx).x, currentVertStart + packedVertByteOffset, sizeof(glm::vec3));
			packedVertByteOffset += sizeof(glm::vec3);

			if (m_canBuildNormals)
			{
				memcpy(&meshData->m_normals->at<glm::vec3>(vertIdx).x, currentVertStart + packedVertByteOffset, sizeof(glm::vec3));
				packedVertByteOffset += sizeof(glm::vec3);
			}
			if (m_canBuildTangents)
			{
				memcpy(&meshData->m_tangents->at<glm::vec4>(vertIdx).x, currentVertStart + packedVertByteOffset, sizeof(glm::vec4));
				packedVertByteOffset += sizeof(glm::vec4);
			}
			if (m_canBuildUVs)
			{
				memcpy(&meshData->m_UV0->at<glm::vec2>(vertIdx).x, currentVertStart + packedVertByteOffset, sizeof(glm::vec2));
				packedVertByteOffset += sizeof(glm::vec2);
			}
			for (auto& extraChannel : *meshData->m_extraChannels)
			{
				void* bytePtr = extraChannel->GetElementPtr(vertIdx);

				const uint8_t elementByteSize = extraChannel->GetElementByteSize();

				memcpy(bytePtr, currentVertStart + packedVertByteOffset, elementByteSize);
				packedVertByteOffset += elementByteSize;
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

		SEAssert(meshData->m_meshParams->m_primitiveTopology == gr::MeshPrimitive::PrimitiveTopology::TriangleList,
			"Only triangular faces are currently supported");
		
		return 3;
	}


	void VertexStreamBuilder::GetPosition(
		const SMikkTSpaceContext* m_context, float* outpos, const int faceIdx, const int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		int index = GetVertexIndex(m_context, faceIdx, vertIdx);
		glm::vec3 const& position = meshData->m_positions->at<glm::vec3>(index);

		outpos[0] = position.x;
		outpos[1] = position.y;
		outpos[2] = position.z;
	}


	void VertexStreamBuilder::GetNormal(
		const SMikkTSpaceContext* m_context, float* outnormal, const int faceIdx, const int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		int index = GetVertexIndex(m_context, faceIdx, vertIdx);
		glm::vec3 const& normal = meshData->m_normals->at<glm::vec3>(index);;

		outnormal[0] = normal.x;
		outnormal[1] = normal.y;
		outnormal[2] = normal.z;
	}


	void VertexStreamBuilder::GetTexCoords(
		const SMikkTSpaceContext* m_context, float* outuv, const int faceIdx, const int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		auto const& index = GetVertexIndex(m_context, faceIdx, vertIdx);
		glm::vec2 const& uv = meshData->m_UV0->at<glm::vec2>(index);

		outuv[0] = uv.x;
		outuv[1] = uv.y;
	}


	void VertexStreamBuilder::SetTangentSpaceBasic(
		const SMikkTSpaceContext* m_context, const float* tangentu, const float fSign, const int faceIdx, const int vertIdx)
	{
		MeshData* meshData = static_cast<MeshData*>(m_context->m_pUserData);

		int index = GetVertexIndex(m_context, faceIdx, vertIdx);
		glm::vec4* tangent = &meshData->m_tangents->at<glm::vec4>(index);

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
		int index = meshData->m_indices->ScalarGetAs<uint32_t>(indicesIdx);

		return index;
	}
}
