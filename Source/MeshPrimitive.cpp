// � 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "MeshPrimitive.h"
#include "MeshPrimitive_Platform.h"
#include "Transform.h"
#include "VertexAttributeBuilder.h"


namespace re
{
	using en::Config;
	using gr::Transform;
	using gr::Bounds;
	using glm::pi;
	using glm::mat4;
	using glm::vec2;
	using glm::vec3;
	using glm::vec4;
	using std::move;
	using std::vector;
	using std::string;
	using std::shared_ptr;


	MeshPrimitive::MeshPrimitive(
		string const& name,
		vector<uint32_t>& indices,
		vector<float>& positions,
		glm::vec3 const& positionMinXYZ,
		glm::vec3 const& positionMaxXYZ,
		vector<float>& normals,
		vector<float>& tangents,
		vector<float>& uv0,
		vector<float>& colors,
		vector<uint8_t> joints,
		vector<float> weights,
		shared_ptr<gr::Material> material,
		MeshPrimitiveParams const& meshParams)
		: NamedObject(name)
		, m_platformParams(nullptr)
		, m_meshMaterial(material)
		, m_params(meshParams)
	{
		platform::MeshPrimitive::CreatePlatformParams(*this);

		m_indices = move(indices);

		m_positions = move(positions);
		m_normals = move(normals);
		m_colors = move(colors);
		m_uv0 = move(uv0);
		m_tangents = move(tangents);

		m_joints = move(joints);
		m_weights = move(weights);

		if (positionMinXYZ == gr::Bounds::k_invalidMinXYZ || positionMaxXYZ == gr::Bounds::k_invalidMaxXYZ)
		{
			// Legacy: Previously, we stored vertex data in vecN types. Instead of rewriting, just cast from floats
			m_localBounds.ComputeBounds(reinterpret_cast<std::vector<vec3> const&>(m_positions));
		}
		else
		{
			m_localBounds = Bounds(positionMinXYZ, positionMaxXYZ);
		}

		ComputeDataHash();
	}


	void MeshPrimitive::Destroy()
	{
		m_indices.clear();

		m_positions.clear();
		m_normals.clear();
		m_tangents.clear();
		m_uv0.clear();
		m_colors.clear();

		m_joints.clear();
		m_weights.clear();

		m_meshMaterial = nullptr;

		platform::MeshPrimitive::Destroy(*this); // Platform-specific destruction
		m_platformParams = nullptr;
	}


	void MeshPrimitive::ComputeDataHash()
	{
		// Material:
		if (m_meshMaterial)
		{
			AddDataBytesToHash(&m_meshMaterial->GetName()[0], m_meshMaterial->GetName().length() * sizeof(char));
		}

		// MeshPrimitive params:
		AddDataBytesToHash(&m_params, sizeof(MeshPrimitiveParams));

		// Vertex data streams:
		if (!m_indices.empty())
		{
			AddDataBytesToHash(&m_indices[0], sizeof(uint32_t) * m_indices.size());
		}
		if (!m_positions.empty())
		{
			AddDataBytesToHash(&m_positions[0], sizeof(float) * m_positions.size());
		}
		if (!m_normals.empty())
		{
			AddDataBytesToHash(&m_normals[0], sizeof(float) * m_normals.size());
		}
		if (!m_tangents.empty())
		{
			AddDataBytesToHash(&m_tangents[0], sizeof(float) * m_tangents.size());
		}
		if (!m_uv0.empty())
		{
			AddDataBytesToHash(&m_uv0[0], sizeof(float) * m_uv0.size());
		}
		if (!m_colors.empty())
		{
			AddDataBytesToHash(&m_colors[0], sizeof(float) * m_colors.size());
		}
		if (!m_joints.empty())
		{
			AddDataBytesToHash(&m_joints[0], sizeof(uint8_t) * m_joints.size());
		}
		if (!m_weights.empty())
		{
			AddDataBytesToHash(&m_weights[0], sizeof(float) * m_weights.size());
		}
	}


	void MeshPrimitive::UpdateBounds(gr::Transform* transform)
	{
		m_localBounds.UpdateAABBBounds(transform);
	}
} // re


namespace meshfactory
{
	using std::vector;
	using glm::vec2;
	using glm::vec3;
	using glm::vec4;
	using re::MeshPrimitive;
	using en::Config;
	
	// TODO: These functions should only create geometry once, and register it with the scene data inventory for reuse


	inline std::shared_ptr<MeshPrimitive> CreateCube()
	{
		// Note: Using a RHCS
		const vector<vec3> positions
		{
			vec3(-1.0f, 1.0f, 1.0f),
			vec3(-1.0f, -1.0f, 1.0f),
			vec3(1.0f, -1.0f, 1.0f),
			vec3(1.0f, 1.0f, 1.0f),
			vec3(-1.0f, 1.0f, -1.0f),
			vec3(-1.0f, -1.0f, -1.0f),
			vec3(1.0f, -1.0f, -1.0f),
			vec3(1.0f, 1.0f, -1.0f)
		};

		// Assemble the vertex data into streams.
		// Debugging hint: position index should = color index. All UV's should be used once per face.		
		vector<vec3> assembledPositions
		{
			positions[0], positions[1], positions[2], positions[3], // Front face
			positions[4], positions[5],	positions[1], positions[0], // Left face
			positions[3], positions[2], positions[6], positions[7], // Right face
			positions[4], positions[0], positions[3], positions[7], // Top face
			positions[1], positions[5],	positions[6], positions[2], // Bottom face
			positions[7], positions[6], positions[5], positions[4]  // Back face
		};

		const vector<vec2> uvs // NOTE: (0,0) = Top left
		{
			vec2(0.0f, 1.0f), // 0
			vec2(0.0f, 0.0f), // 1
			vec2(1.0f, 1.0f), // 2
			vec2(1.0f, 0.0f), // 3
		};

		vector<vec2> assembledUVs
		{
			uvs[1], uvs[0], uvs[2],	uvs[3], // Front face
			uvs[1], uvs[0],	uvs[2],	uvs[3], // Left face
			uvs[1], uvs[0],	uvs[2],	uvs[3], // Right face
			uvs[1], uvs[0],	uvs[2],	uvs[3], // Top face
			uvs[1], uvs[0],	uvs[2],	uvs[3], // Bottom face
			uvs[1], uvs[0],	uvs[2],	uvs[3]  // Back face
		};

		std::vector<uint32_t> cubeIndices // 6 faces * 2 tris * 3 indices 
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
		};

		// Construct any missing vertex attributes for the mesh:
		vector<vec3> normals;
		vector<vec4> tangents;
		vector<vec4> colors;
		std::vector<glm::tvec4<uint8_t>> jointsPlaceholder; // Optional: Will not be generated
		std::vector<glm::vec4> weightsPlaceholder; // Optional: Will not be generated

		constexpr char meshName[] = "cube";

		const MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams;
		util::VertexAttributeBuilder::MeshData meshData
		{
			meshName,
			&defaultMeshPrimitiveParams,
			&cubeIndices,
			&assembledPositions,
			&normals,
			&tangents,
			&assembledUVs,
			&colors,
			&jointsPlaceholder,
			&weightsPlaceholder
		};
		util::VertexAttributeBuilder::BuildMissingVertexAttributes(&meshData);

		// Legacy: Previously, we stored vertex data in vecN types. Instead of rewriting, just cast to float
		return std::make_shared<MeshPrimitive>(
			meshName,
			cubeIndices,
			*reinterpret_cast<vector<float>*>(&assembledPositions),	// Cast our vector<vec3> to vector<float>
			gr::Bounds::k_invalidMinXYZ,
			gr::Bounds::k_invalidMaxXYZ,
			*reinterpret_cast<vector<float>*>(&normals),
			*reinterpret_cast<vector<float>*>(&tangents),
			*reinterpret_cast<vector<float>*>(&assembledUVs),
			*reinterpret_cast<vector<float>*>(&colors),
			std::vector<uint8_t>(), // No joints
			std::vector<float>(), // No weights
			nullptr, // No material
			defaultMeshPrimitiveParams);
	}


	inline std::shared_ptr<MeshPrimitive> CreateFullscreenQuad(ZLocation zLocation)
	{
		float zDepth;
		switch (Config::Get()->GetRenderingAPI())
		{
		case platform::RenderingAPI::OpenGL:
		{
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
		}
		break;
		case platform::RenderingAPI::DX12:
		{
			SEAssertF("DX12 is not yet supported");
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}

		// Create a triangle twice the size of clip space, and let the clipping hardware trim it to size:
		std::vector<vec2> uvs // NOTE: (0,0) = Top left of UV space
		{
			vec2(0.f, -1.f), // tl
			vec2(0.f, 1.f), // bl
			vec2(2.f, 1.f)  // br
		};

		const vec3 tl = vec3(-1.f, 3.f, zDepth);
		const vec3 bl = vec3(-1.f, -1.f, zDepth);
		const vec3 br = vec3(3.0f, -1.0f, zDepth);

		// Assemble geometry:
		std::vector<vec3> positions = { tl, bl, br };
		std::vector<vec4> colors(3, vec4(1, 0, 0, 1)); // Assign a bright red color by default
		std::vector<uint32_t> triIndices{ 0, 1, 2 }; // Note: CCW winding

		// Populate missing data:
		std::vector<vec3> normals;
		std::vector<vec4> tangents;
		std::vector<glm::tvec4<uint8_t>> jointsPlaceholder;
		std::vector<glm::vec4> weightsPlaceholder;

		constexpr char meshName[] = "optimizedFullscreenQuad";

		const MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams; // Use defaults
		util::VertexAttributeBuilder::MeshData meshData
		{
			meshName,
			&defaultMeshPrimitiveParams,
			&triIndices,
			&positions,
			&normals,
			&tangents,
			&uvs,
			&colors,
			&jointsPlaceholder,
			&weightsPlaceholder
		};
		util::VertexAttributeBuilder::BuildMissingVertexAttributes(&meshData);

		return std::make_shared<MeshPrimitive>(
			"optimizedFullscreenQuad",
			triIndices,
			*reinterpret_cast<vector<float>*>(&positions), // Cast our vector<vec3> to vector<float>
			gr::Bounds::k_invalidMinXYZ,
			gr::Bounds::k_invalidMaxXYZ,
			*reinterpret_cast<vector<float>*>(&normals),
			*reinterpret_cast<vector<float>*>(&tangents),
			*reinterpret_cast<vector<float>*>(&uvs),
			*reinterpret_cast<vector<float>*>(&colors),
			std::vector<uint8_t>(), // No joints
			std::vector<float>(), // No weights
			nullptr, // No material
			defaultMeshPrimitiveParams);
	}


	// TODO: Most of the meshfactory functions are still hard-coded for OpenGL spaces
	inline std::shared_ptr<MeshPrimitive> CreateQuad(
		vec3 tl /*= vec3(-0.5f, 0.5f, 0.0f)*/,
		vec3 tr /*= vec3(0.5f, 0.5f, 0.0f)*/,
		vec3 bl /*= vec3(-0.5f, -0.5f, 0.0f)*/,
		vec3 br /*= vec3(0.5f, -0.5f, 0.0f)*/)
	{
		std::vector<vec3> positions = { tl, bl, tr, br };

		std::vector<vec2> uvs // Note: (0,0) = Top left
		{
			vec2(0.f, 0.f), // tl
			vec2(0.f, 1.f), // bl
			vec2(1.f, 0.f), // tr
			vec2(1.f, 1.f)  // br
		};

		std::vector<uint32_t> quadIndices
		{
			0, 1, 2,	// TL face
			2, 1, 3		// BR face
		}; // Note: CCW winding

		std::vector<vec4> colors(4, vec4(1, 0, 0, 1)); // Assign a bright red color by default...

		// Populate missing data:		
		std::vector<vec3> normals;		
		std::vector<vec4> tangents;
		std::vector<glm::tvec4<uint8_t>> jointsPlaceholder;
		std::vector<glm::vec4> weightsPlaceholder;

		constexpr char meshName[] = "quad";

		const MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams;
		util::VertexAttributeBuilder::MeshData meshData
		{
			meshName,
			&defaultMeshPrimitiveParams,
			&quadIndices,
			&positions,
			&normals,
			&tangents,
			&uvs,
			&colors,
			&jointsPlaceholder,
			&weightsPlaceholder
		};
		util::VertexAttributeBuilder::BuildMissingVertexAttributes(&meshData);

		// It's easier to reason about geometry in vecN types; cast to float now we're done
		return std::make_shared<MeshPrimitive>(
			meshName,
			quadIndices,
			*reinterpret_cast<vector<float>*>(&positions), // Cast our vector<vec3> to vector<float>
			gr::Bounds::k_invalidMinXYZ,
			gr::Bounds::k_invalidMaxXYZ,
			*reinterpret_cast<vector<float>*>(&normals),
			*reinterpret_cast<vector<float>*>(&tangents),
			*reinterpret_cast<vector<float>*>(&uvs),
			*reinterpret_cast<vector<float>*>(&colors),
			std::vector<uint8_t>(), // No joints
			std::vector<float>(), // No weights
			nullptr, // No material
			MeshPrimitive::MeshPrimitiveParams());
	}


	inline std::shared_ptr<MeshPrimitive> CreateSphere(
		float radius /*= 0.5f*/,
		size_t numLatSlices /*= 16*/,
		size_t numLongSlices /*= 16*/)
	{
		// NOTE: Some UV's are distorted, as we're using merged vertices. TODO: Fix this

		// Note: Latitude = horizontal lines about Y
		//		Longitude = vertical lines about sphere
		//		numLatSlices = horizontal segments
		//		numLongSlices = vertical segments

		const size_t numVerts = numLatSlices * numLongSlices + 2; // + 2 for end caps
		vector<vec3> positions(numVerts);
		vector<vec3> normals(numVerts);
		vector<vec2> uvs(numVerts);
		
		const size_t numIndices = 3 * numLatSlices * numLongSlices * 2;
		vector<uint32_t> indices(numIndices);

		// Generate a sphere about the Y axis:
		vec3 firstPosition = vec3(0.0f, radius, 0.0f);
		vec3 firstNormal = vec3(0, 1.0f, 0);
		vec3 firstTangent = vec3(0, 0, 0);
		vec2 firstUv0 = vec2(0.5f, 0.0f);

		size_t currentIndex = 0;

		positions[currentIndex] = firstPosition;
		normals[currentIndex] = firstNormal;
		uvs[currentIndex] = firstUv0;
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
		for (int curLongSlices = 0; curLongSlices < numLongSlices; curLongSlices++)
		{
			float y = radius * cos(zRadians);

			// Inner loop: Rotate about Y
			for (int curLatSlices = 0; curLatSlices < numLatSlices; curLatSlices++)
			{
				float x = radius * sin(yRadians) * sin(zRadians);
				float z = radius * cos(yRadians) * sin(zRadians);
				yRadians += yRadianStep;

				vec3 position = vec3(x, y, z);
				vec3 normal = normalize(position);
				vec2 uv0 = vec2(uvX, uvY);

				positions[currentIndex] = position;
				normals[currentIndex] = normal;
				uvs[currentIndex] = uv0;
				currentIndex++;

				uvX += uvXStep;
			}
			yRadians = 0.0f;
			zRadians += zRadianStep;

			uvX = 0;
			uvY += uvYStep;
		}

		// Final endcap:
		const vec3 finalPosition = vec3(0.0f, -radius, 0.0f);
		const vec3 finalNormal = vec3(0, -1, 0);
		const vec2 finalUv0 = vec2(0.5f, 1.0f);

		positions[currentIndex] = finalPosition;
		normals[currentIndex] = finalNormal;
		uvs[currentIndex] = finalUv0;

		// Indices: (Note: We use counter-clockwise vertex winding)
		currentIndex = 0;

		// Top cap:
		for (size_t i = 1; i <= numLatSlices; i++)
		{
			indices[currentIndex++] = (uint32_t)0;
			indices[currentIndex++] = (uint32_t)i;
			indices[currentIndex++] = (uint32_t)(i + 1);
		}
		indices[currentIndex - 1] = 1; // Wrap the last edge back to the start

		// Mid section:
		size_t topLeft = 1;
		size_t topRight = topLeft + 1;
		size_t botLeft = 1 + numLatSlices;
		size_t botRight = botLeft + 1;
		for (size_t i = 0; i < (numLongSlices - 1); i++)
		{
			for (size_t j = 0; j < numLatSlices - 1; j++)
			{
				// Top left triangle:
				indices[currentIndex++] = (uint32_t)topLeft;
				indices[currentIndex++] = (uint32_t)botLeft;
				indices[currentIndex++] = (uint32_t)topRight;

				// Bot right triangle
				indices[currentIndex++] = (uint32_t)topRight;
				indices[currentIndex++] = (uint32_t)botLeft;
				indices[currentIndex++] = (uint32_t)botRight;

				topLeft++;
				topRight++;
				botLeft++;
				botRight++;
			}
			// Wrap the edge around:
			// Top left triangle:
			indices[currentIndex++] = (uint32_t)topLeft;
			indices[currentIndex++] = (uint32_t)botLeft;
			indices[currentIndex++] = (uint32_t)(topRight - numLatSlices);

			// Bot right triangle
			indices[currentIndex++] = (uint32_t)(topRight - numLatSlices);
			indices[currentIndex++] = (uint32_t)botLeft;
			indices[currentIndex++] = (uint32_t)(botRight - numLatSlices);

			// Advance to the next row:
			topLeft++;
			botLeft++;
			topRight++;
			botRight++;
		}

		// Bottom cap:
		for (size_t i = numVerts - numLatSlices - 1; i < numVerts - 1; i++)
		{
			indices[currentIndex++] = (uint32_t)i;
			indices[currentIndex++] = (uint32_t)(numVerts - 1);
			indices[currentIndex++] = (uint32_t)(i + 1);
		}
		indices[currentIndex - 1] = (uint32_t)(numVerts - numLatSlices - 1); // Wrap the last edge back to the start

		// Populate missing data:
		vector<vec4> colors;
		vector<vec4> tangents;
		std::vector<glm::tvec4<uint8_t>> jointsPlaceholder;
		std::vector<glm::vec4> weightsPlaceholder;

		constexpr char meshName[] = "sphere";

		const MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams;
		util::VertexAttributeBuilder::MeshData meshData
		{
			meshName,
			&defaultMeshPrimitiveParams,
			&indices,
			&positions,
			&normals,
			&tangents,
			&uvs,
			&colors,
			&jointsPlaceholder,
			&weightsPlaceholder
		};
		util::VertexAttributeBuilder::BuildMissingVertexAttributes(&meshData);

		// Legacy: Previously, we stored vertex data in vecN types. Instead of rewriting, just cast to float
		return make_shared<MeshPrimitive>(
			meshName,
			indices,
			*reinterpret_cast<vector<float>*>(&positions), // Cast our vector<vec3> to vector<float>
			gr::Bounds::k_invalidMinXYZ,
			gr::Bounds::k_invalidMaxXYZ,
			*reinterpret_cast<vector<float>*>(&normals),
			*reinterpret_cast<vector<float>*>(&tangents),
			*reinterpret_cast<vector<float>*>(&uvs),
			*reinterpret_cast<vector<float>*>(&colors),
			std::vector<uint8_t>(), // No joints
			std::vector<float>(), // No weights
			nullptr, // No material
			defaultMeshPrimitiveParams);
	}
} // meshfactory

