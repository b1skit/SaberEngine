// © 2023 Adam Badke. All rights reserved.
#include "MeshFactory.h"
#include "VertexStreamBuilder.h"

#include "Core/Util/ByteVector.h"


namespace
{
	constexpr float k_minHeight = 0.001f;
	constexpr float k_minRadius = 0.001f;
	constexpr uint32_t k_minSideEdges = 3;


	// Common functionality for creating cyliner-like meshes
	std::shared_ptr<gr::MeshPrimitive> CreateCylinderHelper(
		char const* meshName,
		gr::meshfactory::FactoryOptions const& factoryOptions,
		float height = 1.f,
		float topRadius = 0.5f,
		float botRadius = 0.5f,
		uint32_t numSides = 16,
		bool addTopCap = true)
	{
		height = std::max(std::abs(height), k_minHeight);
		topRadius = std::max(std::abs(topRadius), k_minRadius);
		botRadius = std::max(std::abs(botRadius), k_minRadius);
		numSides = std::max(numSides, k_minSideEdges);

		const uint32_t numTopVerts = (3 * numSides) * addTopCap;
		const uint32_t numBotVerts = 3 * numSides;
		const uint32_t numBodyVerts = 6 * numSides;
		const uint32_t numVerts = numTopVerts + numBotVerts + numBodyVerts;

		util::ByteVector positions = util::ByteVector::Create<glm::vec3>(numVerts);
		util::ByteVector normals = util::ByteVector::Create<glm::vec3>(numVerts);
		util::ByteVector uvs = util::ByteVector::Create<glm::vec2>(numVerts);

		const uint32_t numTopIndices = (3 * numSides) * addTopCap;
		const uint32_t numBotIndices = (3 * numSides);
		const uint32_t numBodyIndices = 6 * numSides;
		const uint32_t numIndices = numTopIndices + numBotIndices + numBodyIndices;
		
		util::ByteVector indices = util::ByteVector::Create<uint16_t>(numIndices);

		const float topYCoord = 0.f;
		const float botYCoord = -height;

		const glm::vec3 topCenterVert = glm::vec3(0.f, 0.f, 0.f);
		const glm::vec3 botCenterVert = glm::vec3(0.f, botYCoord, 0.f);

		const float edgeRadianStep = 2.f * glm::pi<float>() / numSides;

		auto BuildCapVerts = [&](uint32_t& vertInsertIdx, uint32_t& indicesInsertIdx, bool isTopCap)
			{
				const float capHeight = isTopCap ? 0.f : botYCoord;

				const glm::vec3 centerVert = isTopCap ? topCenterVert : botCenterVert;
				const glm::vec3 capNormal = isTopCap ? glm::vec3(0.f, 1.f, 0.f) : glm::vec3(0.f, -1.f, 0.f);
				const glm::vec2 centerVertUV = glm::vec2(0.5f, 0.5f);

				const uint32_t centerVertIndex = vertInsertIdx;

				// Center vertex:
				positions.at<glm::vec3>(vertInsertIdx) = centerVert;
				normals.at<glm::vec3>(vertInsertIdx) = capNormal;
				uvs.at<glm::vec2>(vertInsertIdx) = centerVertUV;
				
				vertInsertIdx++;

				const float radius = isTopCap ? topRadius : botRadius;
				
				constexpr float k_capUVRadius = 0.5f;

				// Edge verts:
				for (uint32_t edgeVertIdx = 0; edgeVertIdx <= numSides; edgeVertIdx++) // <= for the duplicate seam verts
				{
					const float curEdgeRadians = edgeRadianStep * edgeVertIdx;
					positions.at<glm::vec3>(vertInsertIdx) = glm::vec3(
						glm::cos(curEdgeRadians) * radius,
						capHeight,
						glm::sin(curEdgeRadians) * radius * -1.f);

					normals.at<glm::vec3>(vertInsertIdx) = capNormal;

					uvs.at<glm::vec2>(vertInsertIdx) = centerVertUV + glm::vec2(
						glm::cos(edgeRadianStep) * k_capUVRadius,
						glm::sin(edgeRadianStep) * k_capUVRadius);					

					// Indices:
					if (edgeVertIdx < numSides)
					{
						indices.at<uint16_t>(indicesInsertIdx++) = centerVertIndex;
						if (isTopCap)
						{
							indices.at<uint16_t>(indicesInsertIdx++) = vertInsertIdx;
							indices.at<uint16_t>(indicesInsertIdx++) = vertInsertIdx + 1;
						}
						else
						{
							indices.at<uint16_t>(indicesInsertIdx++) = vertInsertIdx + 1;
							indices.at<uint16_t>(indicesInsertIdx++) = vertInsertIdx;
						}
					}

					vertInsertIdx++;
				}
			};

		const uint32_t firstTopCapIdx = 0;
		uint32_t curVertIdx = firstTopCapIdx;
		uint32_t curIndicesIdx = firstTopCapIdx;

		if (addTopCap)
		{
			BuildCapVerts(curVertIdx, curIndicesIdx, true);
		}
		BuildCapVerts(curVertIdx, curIndicesIdx, false);

		const uint32_t firstCylinderBodyVertIdx = curVertIdx; // So we can insert the normals later on...

		// Side verts:
		const float uvStepWidth = 1.f / numSides;
		float curUVX = 0.f;
		for (uint32_t edgeIdx = 0; edgeIdx <= numSides; edgeIdx++) // <= for the duplicate seam verts
		{
			// Indices:
			if (edgeIdx < numSides)
			{
				indices.at<uint16_t>(curIndicesIdx++) = curVertIdx;
				indices.at<uint16_t>(curIndicesIdx++) = curVertIdx + 1;
				indices.at<uint16_t>(curIndicesIdx++) = curVertIdx + 2; // Triangle: |/

				indices.at<uint16_t>(curIndicesIdx++) = curVertIdx + 2;
				indices.at<uint16_t>(curIndicesIdx++) = curVertIdx + 1;
				indices.at<uint16_t>(curIndicesIdx++) = curVertIdx + 3; // Triangle: /|
			}

			const float curEdgeRadians = edgeRadianStep * edgeIdx;

			// Top edge vertex:
			if (edgeIdx < numSides)
			{
				positions.at<glm::vec3>(curVertIdx) = glm::vec3(
					glm::cos(curEdgeRadians) * topRadius,
					topYCoord,
					glm::sin(curEdgeRadians) * topRadius * -1.f);
			}
			else
			{
				positions.at<glm::vec3>(curVertIdx) = positions.at<glm::vec3>(firstCylinderBodyVertIdx);
			}
			
			uvs.at<glm::vec2>(curVertIdx) = glm::vec2(curUVX, 0.f);
			
			curVertIdx++;

			// Bottom edge vertex:
			if (edgeIdx < numSides)
			{
				positions.at<glm::vec3>(curVertIdx) = glm::vec3(
					glm::cos(curEdgeRadians) * botRadius,
					botYCoord,
					glm::sin(curEdgeRadians) * botRadius * -1.f);
			}
			else
			{
				positions.at<glm::vec3>(curVertIdx) = positions.at<glm::vec3>(firstCylinderBodyVertIdx + 1);
			}

			uvs.at<glm::vec2>(curVertIdx) = glm::vec2(curUVX, 1.f);

			curVertIdx++;

			curUVX += uvStepWidth;
		}


		// Normals:
		for (uint32_t edgeIdx = 0; edgeIdx <= numSides; edgeIdx++) // <= for the duplicate seam verts
		{
			const uint32_t normalIdx = firstCylinderBodyVertIdx + (edgeIdx * 2);

			if (edgeIdx < numSides)
			{
				// Direction pointing towards top edge:
				const glm::vec3 edgeDir = 
					glm::normalize(positions.at<glm::vec3>(normalIdx) - positions.at<glm::vec3>(normalIdx + 1));

				// Direction pointing in towards the center:
				const glm::vec3 arbitraryDir = glm::normalize(topCenterVert - positions.at<glm::vec3>(normalIdx));

				const glm::vec3 tangent = glm::cross(edgeDir, arbitraryDir);

				// Walk both the top and bottom vertex in a single step
				normals.at<glm::vec3>(normalIdx) = glm::normalize(glm::cross(edgeDir, tangent));
				normals.at<glm::vec3>(normalIdx + 1) = normals.at<glm::vec3>(normalIdx);
			}
			else
			{
				normals.at<glm::vec3>(normalIdx) = normals.at<glm::vec3>(firstCylinderBodyVertIdx);
				normals.at<glm::vec3>(normalIdx + 1) = normals.at<glm::vec3>(firstCylinderBodyVertIdx + 1);
			}
		}

		util::ByteVector tangents = util::ByteVector::Create<glm::vec4>(); // Empty: Will be generated if necessary
		util::ByteVector colors = util::ByteVector::Create<glm::vec4>(positions.size(), factoryOptions.m_vertexColor);

		const gr::MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams;
		std::vector<util::ByteVector*> extraChannels = { &colors };
		grutil::VertexStreamBuilder::MeshData meshData
		{
			.m_name = meshName,
			.m_meshParams = &defaultMeshPrimitiveParams,
			.m_indices = &indices,
			.m_positions = &positions,
			.m_normals = &normals,
			.m_tangents = factoryOptions.m_generateNormalsAndTangents ? &tangents : nullptr,
			.m_UV0 = &uvs,
			.m_extraChannels = extraChannels.data(),
			.m_numExtraChannels = extraChannels.size(),
		};
		grutil::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

		// Get pointers for our missing attributes, if necessary:
		util::ByteVector* normalsPtr = factoryOptions.m_generateNormalsAndTangents ? &normals : nullptr;
		util::ByteVector* tangentsPtr = factoryOptions.m_generateNormalsAndTangents ? &tangents : nullptr;
		util::ByteVector* colorsPtr = factoryOptions.m_generateNormalsAndTangents ? &colors : nullptr;

		std::vector<re::VertexStream const*> vertexStreams;
		vertexStreams.reserve(4);

		vertexStreams.emplace_back(re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::Position,
				.m_dataType = re::VertexStream::DataType::Float3,
			},
			std::move(positions)).get());
		
		if (normalsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Normal,
					.m_dataType = re::VertexStream::DataType::Float3,
					.m_doNormalize = re::VertexStream::Normalize::True,
				},
				std::move(*normalsPtr)).get());
		}

		if (tangentsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Tangent,
					.m_dataType = re::VertexStream::DataType::Float4,
					.m_doNormalize = re::VertexStream::Normalize::True,
				},
				std::move(*tangentsPtr)).get());
		}

		if (colorsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Color,
					.m_dataType = re::VertexStream::DataType::Float4,
				},
				std::move(*colorsPtr)).get());
		}

		re::VertexStream const* indexStream = re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::Index,
				.m_dataType = re::VertexStream::DataType::UShort,
			},
			std::move(indices)).get();
		

		return gr::MeshPrimitive::Create(
			meshName,
			indexStream,
			std::move(vertexStreams),
			defaultMeshPrimitiveParams);
	}


	void ApplyOrientation(
		util::ByteVector& positions,
		util::ByteVector& normals,
		gr::meshfactory::Orientation orientation)
	{
		if (orientation == gr::meshfactory::Orientation::Default)
		{
			return;
		}

		glm::vec3 lookAtPos(0.f);
		glm::vec3 upDir(0.f);
		switch (orientation)
		{
		case gr::meshfactory::ZNegative:
		{
			lookAtPos = glm::vec3(0.f, -1.f, 0.f);
			upDir = glm::vec3(0.f, 0.f, -1.f);
		}
		break;
		case gr::meshfactory::Default:
		default: SEAssertF("Invalid orientation");
		}

		// We know this matrix doesn't contain any scales/skews, so we can use it for the normals as well.
		// We cast it to mat3 here, since there are no translations involved
		const glm::mat3 lookatMatrix = glm::lookAt(
			glm::vec3(0.f),	// eye
			lookAtPos,		// center
			upDir);			// up

		for (size_t i = 0; i < positions.size(); ++i)
		{
			positions.at<glm::vec3>(i) = lookatMatrix * positions.at<glm::vec3>(i);
		}

		for (size_t i = 0; i < normals.size(); ++i)
		{
			normals.at<glm::vec3>(i) = lookatMatrix * normals.at<glm::vec3>(i);
		}
	}
}


namespace gr::meshfactory
{
	std::shared_ptr<MeshPrimitive> CreateCube(
		FactoryOptions const& factoryOptions /*= FactoryOptions{}*/, float extentDistance /*= 1.f*/)
	{
		extentDistance = std::abs(extentDistance);

		// Note: Using a RHCS
		const std::array<glm::vec3, 8> positionsData
		{
			glm::vec3(-extentDistance, extentDistance, extentDistance),
			glm::vec3(-extentDistance, -extentDistance, extentDistance),
			glm::vec3(extentDistance, -extentDistance, extentDistance),
			glm::vec3(extentDistance, extentDistance, extentDistance),
			glm::vec3(-extentDistance, extentDistance, -extentDistance),
			glm::vec3(-extentDistance, -extentDistance, -extentDistance),
			glm::vec3(extentDistance, -extentDistance, -extentDistance),
			glm::vec3(extentDistance, extentDistance, -extentDistance)
		};

		util::ByteVector assembledPositions = util::ByteVector::Create<glm::vec3>(
		{
			positionsData[0], positionsData[1], positionsData[2], positionsData[3], // Front face
			positionsData[4], positionsData[5],	positionsData[1], positionsData[0], // Left face
			positionsData[3], positionsData[2], positionsData[6], positionsData[7], // Right face
			positionsData[4], positionsData[0], positionsData[3], positionsData[7], // Top face
			positionsData[1], positionsData[5],	positionsData[6], positionsData[2], // Bottom face
			positionsData[7], positionsData[6], positionsData[5], positionsData[4]  // Back face
		});

		const std::vector<glm::vec2> uvs // NOTE: (0,0) = Top left
		{
			glm::vec2(0.0f, 1.0f), // 0
			glm::vec2(0.0f, 0.0f), // 1
			glm::vec2(1.0f, 1.0f), // 2
			glm::vec2(1.0f, 0.0f), // 3
		};

		util::ByteVector assembledUVs = util::ByteVector::Create<glm::vec2>(
		{
			uvs[1], uvs[0], uvs[2],	uvs[3], // Front face
			uvs[1], uvs[0],	uvs[2],	uvs[3], // Left face
			uvs[1], uvs[0],	uvs[2],	uvs[3], // Right face
			uvs[1], uvs[0],	uvs[2],	uvs[3], // Top face
			uvs[1], uvs[0],	uvs[2],	uvs[3], // Bottom face
			uvs[1], uvs[0],	uvs[2],	uvs[3]  // Back face
		});

		util::ByteVector cubeIndices = util::ByteVector::Create<uint16_t>( // 6 faces * 2 tris * 3 indices 
		{
			0, 1, 3, // Front face
			1, 2, 3,

			4, 5, 7, // Left face
			7, 5, 6,

			8, 9, 11, // Right face
			9, 10, 11,

			12, 13, 15, // Top face
			13, 14, 15,

			16, 17, 19, // Bottom face
			17, 18, 19,

			20, 21, 23, // Back face:
			21, 22, 23,
		});

		util::ByteVector normals = util::ByteVector::Create<glm::vec3>(); // Empty: Will be generated if necessary
		util::ByteVector tangents = util::ByteVector::Create<glm::vec4>();
		util::ByteVector colors = 
			util::ByteVector::Create<glm::vec4>(assembledPositions.size(), factoryOptions.m_vertexColor);

		constexpr char const* meshName = "cube";

		const MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams;
		std::vector<util::ByteVector*> extraChannels = { &colors };
		grutil::VertexStreamBuilder::MeshData meshData
		{
			.m_name = meshName,
			.m_meshParams = &defaultMeshPrimitiveParams,
			.m_indices = &cubeIndices,
			.m_positions = &assembledPositions,
			.m_normals = factoryOptions.m_generateNormalsAndTangents ? &normals : nullptr,
			.m_tangents = factoryOptions.m_generateNormalsAndTangents ? &tangents : nullptr,
			.m_UV0 = &assembledUVs,
			.m_extraChannels = extraChannels.data(),
			.m_numExtraChannels = extraChannels.size(),
		};
		grutil::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

		// Get pointers for our missing attributes, if necessary:
		util::ByteVector* normalsPtr = factoryOptions.m_generateNormalsAndTangents ? &normals : nullptr;
		util::ByteVector* tangentsPtr = factoryOptions.m_generateNormalsAndTangents ? &tangents : nullptr;
		util::ByteVector* colorsPtr = factoryOptions.m_generateNormalsAndTangents ? &colors : nullptr;

		std::vector<re::VertexStream const*> vertexStreams;
		vertexStreams.reserve(5);

		vertexStreams.emplace_back(re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::Position,
				.m_dataType = re::VertexStream::DataType::Float3,
			},
			std::move(assembledPositions)).get());

		vertexStreams.emplace_back(re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::TexCoord,
				.m_dataType = re::VertexStream::DataType::Float2,
			},
			std::move(assembledUVs)).get());

		if (normalsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Normal,
					.m_dataType = re::VertexStream::DataType::Float3,
					.m_doNormalize = re::VertexStream::Normalize::True,
				},
				std::move(*normalsPtr)).get());
		}

		if (tangentsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Tangent,
					.m_dataType = re::VertexStream::DataType::Float4,
					.m_doNormalize = re::VertexStream::Normalize::True,
				},
				std::move(*tangentsPtr)).get());
		}

		if (colorsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Color,
					.m_dataType = re::VertexStream::DataType::Float4,
				},
				std::move(*colorsPtr)).get());
		}

		re::VertexStream const* indexStream = re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::Index,
				.m_dataType = re::VertexStream::DataType::UShort,
			},
			std::move(cubeIndices)).get();


		return gr::MeshPrimitive::Create(
			meshName,
			indexStream,
			std::move(vertexStreams),
			defaultMeshPrimitiveParams);
	}


	std::shared_ptr<MeshPrimitive> CreateFullscreenQuad(ZLocation zLocation)
	{
		float zDepth;
		// NOTE: OpenGL & GLM's default clip coordinates have been overridden
		// (via glClipControl/GLM_FORCE_DEPTH_ZERO_TO_ONE)
		switch (zLocation)
		{
		case ZLocation::Near: zDepth = 0.f;
			break;
		case ZLocation::Far: zDepth = 1.f;
			break;
		default: SEAssertF("Invalid Z location");
		}

		// Create a triangle twice the size of clip space, and let the clipping hardware trim it to size:
		util::ByteVector uvs = util::ByteVector::Create<glm::vec2>(// NOTE: (0,0) = Top left of UV space
		{
			glm::vec2(0.f, -1.f), // tl
			glm::vec2(0.f, 1.f), // bl
			glm::vec2(2.f, 1.f)  // br
		});

		const glm::vec3 tl = glm::vec3(-1.f, 3.f, zDepth);
		const glm::vec3 bl = glm::vec3(-1.f, -1.f, zDepth);
		const glm::vec3 br = glm::vec3(3.0f, -1.0f, zDepth);

		// Assemble geometry:
		util::ByteVector positions = util::ByteVector::Create<glm::vec3>({ tl, bl, br });
		util::ByteVector triIndices = util::ByteVector::Create<uint16_t>({ 0, 1, 2 }); // Note: CCW winding

		constexpr char meshName[] = "optimizedFullscreenQuad";

		const MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams; // Use defaults

		grutil::VertexStreamBuilder::MeshData meshData
		{
			.m_name = meshName,
			.m_meshParams = &defaultMeshPrimitiveParams,
			.m_indices = &triIndices,
			.m_positions = &positions,
			.m_normals = nullptr,
			.m_tangents = nullptr,
			.m_UV0 = &uvs,
		};
		grutil::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

		std::vector<re::VertexStream const*> vertexStreams;
		vertexStreams.reserve(2);

		vertexStreams.emplace_back(re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::Position,
				.m_dataType = re::VertexStream::DataType::Float3,
			},
			std::move(positions)).get());

		vertexStreams.emplace_back(re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::TexCoord,
				.m_dataType = re::VertexStream::DataType::Float2,
			},
			std::move(uvs)).get());

		re::VertexStream const* indexStream = re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::Index,
				.m_dataType = re::VertexStream::DataType::UShort,
			},
			std::move(triIndices)).get();

		return gr::MeshPrimitive::Create(
			"optimizedFullscreenQuad",
			indexStream,
			std::move(vertexStreams),
			defaultMeshPrimitiveParams);
	}


	// TODO: Most of the meshfactory functions are still hard-coded for OpenGL spaces
	std::shared_ptr<MeshPrimitive> CreateQuad(
		FactoryOptions const& factoryOptions /*= FactoryOptions{}*/,
		glm::vec3 tl /*= glm::vec3(-0.5f, 0.5f, 0.0f)*/,
		glm::vec3 tr /*= glm::vec3(0.5f, 0.5f, 0.0f)*/,
		glm::vec3 bl /*= glm::vec3(-0.5f, -0.5f, 0.0f)*/,
		glm::vec3 br /*= glm::vec3(0.5f, -0.5f, 0.0f)*/)
	{
		util::ByteVector positions = util::ByteVector::Create<glm::vec3>({ tl, bl, tr, br });

		util::ByteVector uvs = util::ByteVector::Create<glm::vec2>( // Note: (0,0) = Top left
		{
			glm::vec2(0.f, 0.f), // tl
			glm::vec2(0.f, 1.f), // bl
			glm::vec2(1.f, 0.f), // tr
			glm::vec2(1.f, 1.f)  // br
		});

		util::ByteVector quadIndices = util::ByteVector::Create<uint16_t>(
		{
			0, 1, 2,	// TL face
			2, 1, 3		// BR face
		}); // Note: CCW winding

		util::ByteVector normals = util::ByteVector::Create<glm::vec3>(); // Empty: Will be generated if necessary
		util::ByteVector tangents = util::ByteVector::Create<glm::vec4>();
		util::ByteVector colors = util::ByteVector::Create<glm::vec4>(positions.size(), factoryOptions.m_vertexColor);

		constexpr char meshName[] = "quad";

		const MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams;
		std::vector<util::ByteVector*> extraChannels = { &colors };
		grutil::VertexStreamBuilder::MeshData meshData
		{
			.m_name = meshName,
			.m_meshParams = &defaultMeshPrimitiveParams,
			.m_indices = &quadIndices,
			.m_positions = &positions,
			.m_normals = factoryOptions.m_generateNormalsAndTangents ? &normals : nullptr,
			.m_tangents = factoryOptions.m_generateNormalsAndTangents ? &tangents : nullptr,
			.m_UV0 = &uvs,
			.m_extraChannels = extraChannels.data(),
			.m_numExtraChannels = extraChannels.size(),
		};
		grutil::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

		// Get pointers for our missing attributes, if necessary:
		util::ByteVector* normalsPtr = factoryOptions.m_generateNormalsAndTangents ? &normals : nullptr;
		util::ByteVector* tangentsPtr = factoryOptions.m_generateNormalsAndTangents ? &tangents : nullptr;
		util::ByteVector* colorsPtr = factoryOptions.m_generateNormalsAndTangents ? &colors : nullptr;

		std::vector<re::VertexStream const*> vertexStreams;
		vertexStreams.reserve(5);

		vertexStreams.emplace_back(re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::Position,
				.m_dataType = re::VertexStream::DataType::Float3,
			},
			std::move(positions)).get());

		vertexStreams.emplace_back(re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::TexCoord,
				.m_dataType = re::VertexStream::DataType::Float2,
			},
			std::move(uvs)).get());

		if (normalsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Normal,
					.m_dataType = re::VertexStream::DataType::Float3,
					.m_doNormalize = re::VertexStream::Normalize::True,
				},
				std::move(*normalsPtr)).get());
		}

		if (tangentsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Tangent,
					.m_dataType = re::VertexStream::DataType::Float4,
					.m_doNormalize = re::VertexStream::Normalize::True,
				},
				std::move(*tangentsPtr)).get());
		}

		if (colorsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Color,
					.m_dataType = re::VertexStream::DataType::Float4,
				},
				std::move(*colorsPtr)).get());
		}

		re::VertexStream const* indexStream = re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::Index,
				.m_dataType = re::VertexStream::DataType::UShort,
			},
			std::move(quadIndices)).get();

		return gr::MeshPrimitive::Create(
			meshName,
			indexStream,
			std::move(vertexStreams),
			MeshPrimitive::MeshPrimitiveParams{});
	}


	std::shared_ptr<gr::MeshPrimitive> CreateQuad(
		FactoryOptions const& factoryOptions /*= FactoryOptions{}*/,
		float extentDistance /*= 0.5f*/)
	{
		extentDistance = std::abs(extentDistance);

		return CreateQuad(
			factoryOptions,
			glm::vec3(-extentDistance, extentDistance, 0.0f),
			glm::vec3(extentDistance, extentDistance, 0.0f),
			glm::vec3(-extentDistance, -extentDistance, 0.0f),
			glm::vec3(extentDistance, -extentDistance, 0.0f));
	}

	
	std::shared_ptr<MeshPrimitive> CreateSphere(
		FactoryOptions const& factoryOptions /*= FactoryOptions{}*/,
		float radius /*= 0.5f*/,
		uint32_t numLatSlices /*= 16*/,
		uint32_t numLongSlices /*= 16*/)
	{
		radius = std::max(std::abs(radius), k_minRadius);
		numLatSlices = std::max(numLatSlices, k_minSideEdges);
		numLongSlices = std::max(numLongSlices, k_minSideEdges);

		// NOTE: Some UV's are distorted, as we're using merged vertices. TODO: Fix this

		// Note: Latitude = horizontal lines about Y
		//		Longitude = vertical lines about sphere
		//		numLatSlices = horizontal segments
		//		numLongSlices = vertical segments

		const uint32_t numVerts = numLatSlices * numLongSlices + 2; // + 2 for end caps
		util::ByteVector positions = util::ByteVector::Create<glm::vec3>(numVerts);
		util::ByteVector normals = util::ByteVector::Create<glm::vec3>(numVerts);
		util::ByteVector uvs = util::ByteVector::Create<glm::vec2>(numVerts);

		const uint32_t numIndices = 3 * numLatSlices * numLongSlices * 2;
		util::ByteVector indices = util::ByteVector::Create<uint16_t>(numIndices);

		// Generate a sphere about the Y axis:
		glm::vec3 firstPosition = glm::vec3(0.0f, radius, 0.0f);
		glm::vec3 firstNormal = glm::vec3(0, 1.0f, 0);
		glm::vec3 firstTangent = glm::vec3(0, 0, 0);
		glm::vec2 firstUv0 = glm::vec2(0.5f, 0.0f);

		uint32_t currentIndex = 0;

		positions.at<glm::vec3>(currentIndex) = firstPosition;
		normals.at<glm::vec3>(currentIndex) = firstNormal;
		uvs.at<glm::vec2>(currentIndex) = firstUv0;
		currentIndex++;

		// Rotate about Z: Arc down the side profile of our sphere
		// cos theta = adj/hyp -> hyp * cos theta = adj -> radius * cos theta = Y
		float zRadianStep = glm::pi<float>() / (float)(numLongSlices + 1); // +1 to get the number of rows
		float zRadians = zRadianStep; // Already added cap vertex, so start on the next step

		// Rotate about Y: Horizontal edges
		// sin theta = opp/hyp -> hyp * sin theta = opp -> radius * sin theta = X
		// cos theta = adj/hyp -> hyp * cos theta = adj -> radius * cos theta = Z
		float yRadianStep = (2.0f * glm::pi<float>()) / (float)numLatSlices; //
		float yRadians = 0.0f;

		// Build UV's, from top left (0,0) to bottom right (1,1)
		float uvXStep = 1.0f / (float)numLatSlices;
		float uvYStep = 1.0f / (float)(numLongSlices + 1);
		float uvX = 0;
		float uvY = uvYStep;

		// Outer loop: Rotate about Z, tracing the arc of the side silhouette down the Y axis
		for (uint32_t curLongSlices = 0; curLongSlices < numLongSlices; curLongSlices++)
		{
			float y = radius * cos(zRadians);

			// Inner loop: Rotate about Y
			for (uint32_t curLatSlices = 0; curLatSlices < numLatSlices; curLatSlices++)
			{
				float x = radius * sin(yRadians) * sin(zRadians);
				float z = radius * cos(yRadians) * sin(zRadians);
				yRadians += yRadianStep;

				glm::vec3 position = glm::vec3(x, y, z);
				glm::vec3 normal = glm::normalize(position);
				glm::vec2 uv0 = glm::vec2(uvX, uvY);

				positions.at<glm::vec3>(currentIndex) = position;
				normals.at<glm::vec3>(currentIndex) = normal;
				uvs.at<glm::vec2>(currentIndex) = uv0;
				currentIndex++;

				uvX += uvXStep;
			}
			yRadians = 0.0f;
			zRadians += zRadianStep;

			uvX = 0;
			uvY += uvYStep;
		}

		// Final endcap:
		const glm::vec3 finalPosition = glm::vec3(0.0f, -radius, 0.0f);
		const glm::vec3 finalNormal = glm::vec3(0, -1, 0);
		const glm::vec2 finalUv0 = glm::vec2(0.5f, 1.0f);

		positions.at<glm::vec3>(currentIndex) = finalPosition;
		normals.at<glm::vec3>(currentIndex) = finalNormal;
		uvs.at<glm::vec2>(currentIndex) = finalUv0;

		// Indices: (Note: We use counter-clockwise vertex winding)
		currentIndex = 0;

		// Top cap:
		for (uint32_t i = 1; i <= numLatSlices; i++)
		{
			indices.at<uint16_t>(currentIndex++) = 0;
			indices.at<uint16_t>(currentIndex++) = i;
			indices.at<uint16_t>(currentIndex++) = i + 1;
		}
		indices.at<uint16_t>(currentIndex - 1) = 1; // Wrap the last edge back to the start

		// Mid section:
		uint32_t topLeft = 1;
		uint32_t topRight = topLeft + 1;
		uint32_t botLeft = 1 + numLatSlices;
		uint32_t botRight = botLeft + 1;
		for (uint32_t i = 0; i < (numLongSlices - 1); i++)
		{
			for (uint32_t j = 0; j < numLatSlices - 1; j++)
			{
				// Top left triangle:
				indices.at<uint16_t>(currentIndex++) = topLeft;
				indices.at<uint16_t>(currentIndex++) = botLeft;
				indices.at<uint16_t>(currentIndex++) = topRight;

				// Bot right triangle
				indices.at<uint16_t>(currentIndex++) = topRight;
				indices.at<uint16_t>(currentIndex++) = botLeft;
				indices.at<uint16_t>(currentIndex++) = botRight;

				topLeft++;
				topRight++;
				botLeft++;
				botRight++;
			}
			// Wrap the edge around:
			// Top left triangle:
			indices.at<uint16_t>(currentIndex++) = topLeft;
			indices.at<uint16_t>(currentIndex++) = botLeft;
			indices.at<uint16_t>(currentIndex++) = topRight - numLatSlices;

			// Bot right triangle
			indices.at<uint16_t>(currentIndex++) = topRight - numLatSlices;
			indices.at<uint16_t>(currentIndex++) = botLeft;
			indices.at<uint16_t>(currentIndex++) = botRight - numLatSlices;

			// Advance to the next row:
			topLeft++;
			botLeft++;
			topRight++;
			botRight++;
		}

		// Bottom cap:
		for (uint32_t i = numVerts - numLatSlices - 1; i < numVerts - 1; i++)
		{
			indices.at<uint16_t>(currentIndex++) = i;
			indices.at<uint16_t>(currentIndex++) = numVerts - 1;
			indices.at<uint16_t>(currentIndex++) = i + 1;
		}
		indices.at<uint16_t>(currentIndex - 1) = numVerts - numLatSlices - 1; // Wrap the last edge back to the start

		constexpr char meshName[] = "sphere";

		util::ByteVector tangents = util::ByteVector::Create<glm::vec4>(); // Empty: Will be generated if necessary
		util::ByteVector colors = util::ByteVector::Create<glm::vec4>(positions.size(), factoryOptions.m_vertexColor);

		const MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams;
		std::vector<util::ByteVector*> extraChannels = { &colors };
		grutil::VertexStreamBuilder::MeshData meshData
		{
			.m_name = meshName,
			.m_meshParams = &defaultMeshPrimitiveParams,
			.m_indices = &indices,
			.m_positions = &positions,
			.m_normals = factoryOptions.m_generateNormalsAndTangents ? &normals : nullptr,
			.m_tangents = factoryOptions.m_generateNormalsAndTangents ? &tangents : nullptr,
			.m_UV0 = &uvs,
			.m_extraChannels = extraChannels.data(),
			.m_numExtraChannels = extraChannels.size(),
		};
		grutil::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

		// Get pointers for our missing attributes, if necessary:
		util::ByteVector* normalsPtr = factoryOptions.m_generateNormalsAndTangents ? &normals : nullptr;
		util::ByteVector* tangentsPtr = factoryOptions.m_generateNormalsAndTangents ? &tangents : nullptr;
		util::ByteVector* colorsPtr = factoryOptions.m_generateNormalsAndTangents ? &colors : nullptr;

		std::vector<re::VertexStream const*> vertexStreams;
		vertexStreams.reserve(4);

		vertexStreams.emplace_back(re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::Position,
				.m_dataType = re::VertexStream::DataType::Float3,
			},
			std::move(positions)).get());

		vertexStreams.emplace_back(re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::TexCoord,
				.m_dataType = re::VertexStream::DataType::Float2,
			},
			std::move(uvs)).get());

		if (normalsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Normal,
					.m_dataType = re::VertexStream::DataType::Float3,
					.m_doNormalize = re::VertexStream::Normalize::True,
				},
				std::move(*normalsPtr)).get());
		}

		if (tangentsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Tangent,
					.m_dataType = re::VertexStream::DataType::Float4,
					.m_doNormalize = re::VertexStream::Normalize::True,
				},
				std::move(*tangentsPtr)).get());
		}

		if (colorsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Color,
					.m_dataType = re::VertexStream::DataType::Float4,
				},
				std::move(*colorsPtr)).get());
		}

		re::VertexStream const* indexStream = re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::Index,
				.m_dataType = re::VertexStream::DataType::UShort,
			},
			std::move(indices)).get();

		return gr::MeshPrimitive::Create(
			meshName,
			indexStream,
			std::move(vertexStreams),
			defaultMeshPrimitiveParams);
	}


	std::shared_ptr<gr::MeshPrimitive> CreateCone(
		FactoryOptions const& factoryOptions /*= FactoryOptions{}*/,
		float height /*= 1.f*/,
		float radius /*= 0.5f*/,
		uint32_t numSides /*= 16*/)
	{
		height = std::max(std::abs(height), k_minHeight);
		radius = std::max(std::abs(radius), k_minRadius);
		numSides = std::max(numSides, k_minSideEdges);

		// Unique top verts per side face, shared non-seam edge verts per side face, shared non-seam edge verts per
		// bottom face, shared bottom center point
		const uint32_t numVerts = numSides + ((numSides + 1) * 2) + 1; // + 1 for shared bottom center point

		util::ByteVector positions = util::ByteVector::Create<glm::vec3>(numVerts);
		util::ByteVector normals = util::ByteVector::Create<glm::vec3>(numVerts);
		util::ByteVector uvs = util::ByteVector::Create<glm::vec2>(numVerts);

		const uint32_t numIndices = 3 * 2 * numSides; // 3 indices per triangle, with 2 triangles per side/base step
		util::ByteVector indices = util::ByteVector::Create<uint16_t>(numIndices);

		const float yCoord = -height;
		const glm::vec3 topPosition = glm::vec3(0.f); // We need a unique top vert per side face

		// We pack the vertices like so: {t, t, ..., t, s, s, ..., s, b, b, ..., b, c}, for
		// t = top verts, s = side edge verts, b = bottom edge verts, c = shared bottom center vert
		const uint32_t numTopVerts = numSides;
		const uint32_t firstTopVertIdx = 0;
		const uint32_t lastTopVertIdx = numTopVerts - 1;
		uint32_t topVertIdx = firstTopVertIdx;
		
		const uint32_t numSideEdgeVerts = numSides + 1; // +1 for the duplicate seam vert
		const uint32_t firstSideEdgeVertIdx = numTopVerts;
		const uint32_t lastSideEdgeVertIdx = numTopVerts + numSideEdgeVerts - 1;
		uint32_t sideEdgeVertIdx = firstSideEdgeVertIdx;

		const uint32_t numBottomEdgeVerts = numSides + 1; // +1 for the duplicate seam vert
		const uint32_t firstBottomEdgeVertIdx = lastSideEdgeVertIdx + 1;
		const uint32_t lastBottomEdgeVertIdx = numTopVerts + numSideEdgeVerts + numBottomEdgeVerts - 1;
		uint32_t bottomEdgeVertIdx = firstBottomEdgeVertIdx;

		const uint32_t bottomVertIdx = numVerts - 1;

		// Note: Currently, the side faces are laid out like a fan in UV space with the tip of the cone in the top-right
		// corner at (1,0), and an edge length of 1 in UV space. The bottom disk is centered in the middle of UV space
		// at (0.5, 0.5), with a diameter of 1 in UV space. Thus, the UV islands overlap for now...
		const glm::vec2 topVertUV = glm::vec2(1.f, 0.f);
		const float faceEdgeUVLength = 1.f;
		const glm::vec2 bottomCenterVertUV = glm::vec2(0.5f, 0.5f);
		const float bottomEdgeUVLength = 0.5f;

		uint32_t indicesIdx = 0;

		const float edgeRadianStep = 2.f * glm::pi<float>() / numSides;
		const float faceUVRadianStep = 0.5f * glm::pi<float>() / numSides;
		const float bottomUVRadianStep = 2.f * glm::pi<float>() / numSides;

		for (uint32_t edgeIdx = 0; edgeIdx < (numSides + 1); edgeIdx++)
		{
			if (edgeIdx == numSides)
			{
				// No extra top vert is necessary
				positions.at<glm::vec3>(sideEdgeVertIdx) = positions.at<glm::vec3>(firstSideEdgeVertIdx);
				positions.at<glm::vec3>(bottomEdgeVertIdx) = positions.at<glm::vec3>(firstBottomEdgeVertIdx);
			}
			else
			{
				// Top point:
				positions.at<glm::vec3>(topVertIdx) = topPosition;

				// Cone edge vertex:
				const float curRadians = edgeIdx * edgeRadianStep;
				const float xCoord = radius * cos(curRadians);
				const float zCoord = radius * sin(curRadians) * -1.f;
				const glm::vec3 edgePosition = glm::vec3(xCoord, yCoord, zCoord);

				positions.at<glm::vec3>(sideEdgeVertIdx) = edgePosition; // Side face edge
				positions.at<glm::vec3>(bottomEdgeVertIdx) = edgePosition; // Bottom face edge

				// UVs:
				const float curFaceUVRadians = glm::pi<float>() + (edgeIdx * faceUVRadianStep);
				uvs.at<glm::vec2>(topVertIdx) = topVertUV;
				uvs.at<glm::vec2>(sideEdgeVertIdx) = 
					topVertUV + glm::vec2(glm::cos(curFaceUVRadians), glm::sin(curFaceUVRadians)) * faceEdgeUVLength;
				
				const float curBotUVRadians = edgeIdx * bottomUVRadianStep;
				uvs.at<glm::vec2>(bottomEdgeVertIdx) = 
					bottomCenterVertUV + glm::vec2(glm::cos(curBotUVRadians), glm::sin(curBotUVRadians)) * bottomEdgeUVLength;

				// Indices:
				const uint32_t nextSideEdgeVertIdx = sideEdgeVertIdx + 1;
				const uint32_t nextBotEdgeVertIdx = bottomEdgeVertIdx + 1;

				// Side face:
				indices.at<uint16_t>(indicesIdx++) = topVertIdx;
				indices.at<uint16_t>(indicesIdx++) = sideEdgeVertIdx;
				indices.at<uint16_t>(indicesIdx++) = nextSideEdgeVertIdx;

				// Bottom face:
				indices.at<uint16_t>(indicesIdx++) = nextBotEdgeVertIdx;
				indices.at<uint16_t>(indicesIdx++) = bottomEdgeVertIdx;
				indices.at<uint16_t>(indicesIdx++) = bottomVertIdx;
			}

			// Prepare for the next iteration:
			topVertIdx++;
			sideEdgeVertIdx++;
			bottomEdgeVertIdx++;
		}

		// Shared bottom center vertex:
		positions.at<glm::vec3>(bottomVertIdx) = glm::vec3(0.f, yCoord, 0.f);
		uvs.at<glm::vec2>(bottomVertIdx) = bottomCenterVertUV;
		
		// Soft normals:
		
		if (factoryOptions.m_generateNormalsAndTangents)
		{
			// Top vertices:
			for (uint32_t vertIdx = 0; vertIdx < numTopVerts; vertIdx++)
			{
				const uint32_t topVertIdx = firstTopVertIdx + vertIdx;
				const uint32_t blVertIdx = firstSideEdgeVertIdx + vertIdx;
				const uint32_t brVertIdx = blVertIdx + 1;

				const glm::vec3 tangentX = positions.at<glm::vec3>(brVertIdx) - positions.at<glm::vec3>(blVertIdx);
				const glm::vec3 bitangentY = positions.at<glm::vec3>(topVertIdx) - positions.at<glm::vec3>(blVertIdx);

				normals.at<glm::vec3>(topVertIdx) = glm::normalize(glm::cross(tangentX, bitangentY));
			}

			// Side edge normals:
			uint32_t leftTopVertIdx = lastTopVertIdx;
			uint32_t rightTopVertIdx = firstTopVertIdx;
			for (uint32_t vertIdx = 0; vertIdx < numSideEdgeVerts; vertIdx++)
			{
				const uint32_t normalIdx = firstSideEdgeVertIdx + vertIdx;

				normals.at<glm::vec3>(normalIdx) = 
					glm::normalize((normals.at<glm::vec3>(leftTopVertIdx) + normals.at<glm::vec3>(rightTopVertIdx)) * 0.5f);

				leftTopVertIdx = (leftTopVertIdx + 1) % numTopVerts;
				rightTopVertIdx = (rightTopVertIdx + 1) % numTopVerts;
			}

			// Bottom vertex normals:
			const glm::vec3 bottomNormal = glm::vec3(0.f, -1.f, 0.f);
			normals.at<glm::vec3>(bottomVertIdx) = bottomNormal;
			for (uint32_t vertIdx = 0; vertIdx < numBottomEdgeVerts; vertIdx++)
			{
				const uint32_t botVertIdx = firstBottomEdgeVertIdx + vertIdx;
				normals.at<glm::vec3>(botVertIdx) = bottomNormal;
			}
		}

		// Apply the orientation before we generate any additional parameters:
		ApplyOrientation(positions, normals, factoryOptions.m_orientation);

		constexpr char const* meshName = "cone";

		util::ByteVector tangents = util::ByteVector::Create<glm::vec4>(); // Empty: Will be generated if necessary
		util::ByteVector colors = util::ByteVector::Create<glm::vec4>(positions.size(), factoryOptions.m_vertexColor);

		const MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams;
		std::vector<util::ByteVector*> extraChannels = { &colors };
		grutil::VertexStreamBuilder::MeshData meshData
		{
			.m_name = meshName,
			.m_meshParams = &defaultMeshPrimitiveParams,
			.m_indices = &indices,
			.m_positions = &positions,
			.m_normals = factoryOptions.m_generateNormalsAndTangents ? &normals : nullptr,
			.m_tangents = factoryOptions.m_generateNormalsAndTangents ? &tangents : nullptr,
			.m_UV0 = &uvs,
			.m_extraChannels = extraChannels.data(),
			.m_numExtraChannels = extraChannels.size(),
		};
		grutil::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

		// Get pointers for our missing attributes, if necessary:
		util::ByteVector* normalsPtr = factoryOptions.m_generateNormalsAndTangents ? &normals : nullptr;
		util::ByteVector* tangentsPtr = factoryOptions.m_generateNormalsAndTangents ? &tangents : nullptr;
		util::ByteVector* colorsPtr = factoryOptions.m_generateNormalsAndTangents ? &colors : nullptr;

		std::vector<re::VertexStream const*> vertexStreams;
		vertexStreams.reserve(5);

		vertexStreams.emplace_back(re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::Position,
				.m_dataType = re::VertexStream::DataType::Float3,
			},
			std::move(positions)).get());

		vertexStreams.emplace_back(re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::TexCoord,
				.m_dataType = re::VertexStream::DataType::Float2,
			},
			std::move(uvs)).get());

		if (normalsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Normal,
					.m_dataType = re::VertexStream::DataType::Float3,
					.m_doNormalize = re::VertexStream::Normalize::True,
				},
				std::move(*normalsPtr)).get());
		}

		if (tangentsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Tangent,
					.m_dataType = re::VertexStream::DataType::Float4,
					.m_doNormalize = re::VertexStream::Normalize::True,
				},
				std::move(*tangentsPtr)).get());
		}

		if (colorsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Color,
					.m_dataType = re::VertexStream::DataType::Float4,
				},
				std::move(*colorsPtr)).get());
		}

		re::VertexStream const* indexStream = re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::Index,
				.m_dataType = re::VertexStream::DataType::UShort,
			},
			std::move(indices)).get();

		return gr::MeshPrimitive::Create(
			meshName,
			indexStream,
			std::move(vertexStreams),
			defaultMeshPrimitiveParams);
	}


	std::shared_ptr<gr::MeshPrimitive> CreateCylinder(
		FactoryOptions const& factoryOptions /*= FactoryOptions{}*/,
		float height /*= 1.f*/,
		float radius /*= 0.5f*/,
		uint32_t numSides /*= 16*/)
	{
		return CreateCylinderHelper("cylinder", factoryOptions, height, radius, radius, numSides, true);
	}

	std::shared_ptr<gr::MeshPrimitive> CreateHelloTriangle(
		FactoryOptions const& factoryOptions /*= FactoryOptions{}*/,
		float scale /*= 1.f*/, 
		float zDepth /*= 0.5f*/)
	{
		zDepth = std::clamp(zDepth, 0.f, 1.f);

		util::ByteVector positions = util::ByteVector::Create<glm::vec3>( // In clip space: bl near = [-1,-1, 0] , tr far = [1,1,1]
		{
			glm::vec3(0.0f * scale,		0.75f * scale,	zDepth),	// Top center
			glm::vec3(-0.75f * scale,	-0.75f * scale, zDepth),	// bl
			glm::vec3(0.75f * scale,	-0.75f * scale, zDepth)		// br
		});

		util::ByteVector uvs = util::ByteVector::Create<glm::vec2>( // Note: (0,0) = Top left
		{
			glm::vec2(0.5f, 0.f),	// Top center
			glm::vec2(0.f, 1.f),	// bl
			glm::vec2(1.f, 0.f),	// br
		});

		util::ByteVector indices = util::ByteVector::Create<uint16_t>(
		{
			0, 1, 2
		}); // Note: CCW winding

		util::ByteVector colors = util::ByteVector::Create<glm::vec4>({
			glm::vec4(1.f, 0.f, 0.f, 1.f), // Top center: Red
			glm::vec4(0.f, 1.f, 0.f, 1.f), // bl: Green
			glm::vec4(0.f, 0.f, 1.f, 1.f), // br: Blue
		});

		util::ByteVector normals = util::ByteVector::Create<glm::vec3>(); // Empty: Will be generated if necessary
		util::ByteVector tangents = util::ByteVector::Create<glm::vec4>();
		std::vector<util::ByteVector*> extraChannels = { &colors };
		constexpr char meshName[] = "helloTriangle";

		const MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams;
		grutil::VertexStreamBuilder::MeshData meshData
		{
			.m_name = meshName,
			.m_meshParams = &defaultMeshPrimitiveParams,
			.m_indices = &indices,
			.m_positions = &positions,
			.m_normals = factoryOptions.m_generateNormalsAndTangents ? &normals : nullptr,
			.m_tangents = factoryOptions.m_generateNormalsAndTangents ? &tangents : nullptr,
			.m_UV0 = &uvs,
			.m_extraChannels = extraChannels.data(),
			.m_numExtraChannels = extraChannels.size(),
		};
		grutil::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

		// Get pointers for our missing attributes, if necessary:
		util::ByteVector* normalsPtr = factoryOptions.m_generateNormalsAndTangents ? &normals : nullptr;
		util::ByteVector* tangentsPtr = factoryOptions.m_generateNormalsAndTangents ? &tangents : nullptr;

		std::vector<re::VertexStream const*> vertexStreams;
		vertexStreams.reserve(5);

		vertexStreams.emplace_back(re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::Position,
				.m_dataType = re::VertexStream::DataType::Float3,
			},
			std::move(positions)).get());

		vertexStreams.emplace_back(re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::TexCoord,
				.m_dataType = re::VertexStream::DataType::Float2,
			},
			std::move(uvs)).get());

		if (normalsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Normal,
					.m_dataType = re::VertexStream::DataType::Float3,
					.m_doNormalize = re::VertexStream::Normalize::True,
				},
				std::move(*normalsPtr)).get());
		}

		if (tangentsPtr)
		{
			vertexStreams.emplace_back(re::VertexStream::Create(
				re::VertexStream::CreateParams{
					.m_type = re::VertexStream::Type::Tangent,
					.m_dataType = re::VertexStream::DataType::Float4,
					.m_doNormalize = re::VertexStream::Normalize::True,
				},
				std::move(*tangentsPtr)).get());
		}

		vertexStreams.emplace_back(re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::Color,
				.m_dataType = re::VertexStream::DataType::Float4,
			},
			std::move(colors)).get());

		re::VertexStream const* indexStream = re::VertexStream::Create(
			re::VertexStream::CreateParams{
				.m_type = re::VertexStream::Type::Index,
				.m_dataType = re::VertexStream::DataType::UShort,
			},
			std::move(indices)).get();

		return gr::MeshPrimitive::Create(
			meshName,
			indexStream,
			std::move(vertexStreams),
			defaultMeshPrimitiveParams);
	}
}