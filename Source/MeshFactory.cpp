// © 2023 Adam Badke. All rights reserved.
#include "MeshFactory.h"
#include "VertexStreamBuilder.h"


namespace gr::meshfactory
{
	inline std::shared_ptr<MeshPrimitive> CreateCube()
	{
		// Note: Using a RHCS
		const std::vector<glm::vec3> positions
		{
			glm::vec3(-1.0f, 1.0f, 1.0f),
			glm::vec3(-1.0f, -1.0f, 1.0f),
			glm::vec3(1.0f, -1.0f, 1.0f),
			glm::vec3(1.0f, 1.0f, 1.0f),
			glm::vec3(-1.0f, 1.0f, -1.0f),
			glm::vec3(-1.0f, -1.0f, -1.0f),
			glm::vec3(1.0f, -1.0f, -1.0f),
			glm::vec3(1.0f, 1.0f, -1.0f)
		};

		// Assemble the vertex data into streams.
		// Debugging hint: position index should = color index. All UV's should be used once per face.		
		std::vector<glm::vec3> assembledPositions
		{
			positions[0], positions[1], positions[2], positions[3], // Front face
			positions[4], positions[5],	positions[1], positions[0], // Left face
			positions[3], positions[2], positions[6], positions[7], // Right face
			positions[4], positions[0], positions[3], positions[7], // Top face
			positions[1], positions[5],	positions[6], positions[2], // Bottom face
			positions[7], positions[6], positions[5], positions[4]  // Back face
		};

		const std::vector<glm::vec2> uvs // NOTE: (0,0) = Top left
		{
			glm::vec2(0.0f, 1.0f), // 0
			glm::vec2(0.0f, 0.0f), // 1
			glm::vec2(1.0f, 1.0f), // 2
			glm::vec2(1.0f, 0.0f), // 3
		};

		std::vector<glm::vec2> assembledUVs
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

		constexpr char meshName[] = "cube";

		const MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams;
		util::VertexStreamBuilder::MeshData meshData
		{
			meshName,
			&defaultMeshPrimitiveParams,
			&cubeIndices,
			&assembledPositions,
			nullptr,
			nullptr,
			&assembledUVs,
			nullptr,
			nullptr,
			nullptr };
		util::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

		// Legacy: Previously, we stored vertex data in vecN types. Instead of rewriting, just cast to float
		return MeshPrimitive::Create(
			meshName,
			&cubeIndices,
			*reinterpret_cast<std::vector<float>*>(&assembledPositions),	// Cast our vector<glm::vec3> to vector<float>
			gr::Bounds::k_invalidMinXYZ,
			gr::Bounds::k_invalidMaxXYZ,
			nullptr,
			nullptr,
			reinterpret_cast<std::vector<float>*>(&assembledUVs),
			nullptr,
			nullptr, // No joints
			nullptr, // No weights
			nullptr, // No material
			defaultMeshPrimitiveParams);
	}


	inline std::shared_ptr<MeshPrimitive> CreateFullscreenQuad(ZLocation zLocation)
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
		std::vector<glm::vec2> uvs // NOTE: (0,0) = Top left of UV space
		{
			glm::vec2(0.f, -1.f), // tl
			glm::vec2(0.f, 1.f), // bl
			glm::vec2(2.f, 1.f)  // br
		};

		const glm::vec3 tl = glm::vec3(-1.f, 3.f, zDepth);
		const glm::vec3 bl = glm::vec3(-1.f, -1.f, zDepth);
		const glm::vec3 br = glm::vec3(3.0f, -1.0f, zDepth);

		// Assemble geometry:
		std::vector<glm::vec3> positions = { tl, bl, br };
		std::vector<uint32_t> triIndices{ 0, 1, 2 }; // Note: CCW winding

		constexpr char meshName[] = "optimizedFullscreenQuad";

		const MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams; // Use defaults
		util::VertexStreamBuilder::MeshData meshData
		{
			meshName,
			&defaultMeshPrimitiveParams,
			&triIndices,
			&positions,
			nullptr,
			nullptr,
			&uvs,
			nullptr,
			nullptr,
			nullptr
		};
		util::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

		return MeshPrimitive::Create(
			"optimizedFullscreenQuad",
			&triIndices,
			*reinterpret_cast<std::vector<float>*>(&positions), // Cast our vector<glm::vec3> to vector<float>
			gr::Bounds::k_invalidMinXYZ,
			gr::Bounds::k_invalidMaxXYZ,
			nullptr,
			nullptr,
			reinterpret_cast<std::vector<float>*>(&uvs),
			nullptr,
			nullptr, // No joints
			nullptr, // No weights
			nullptr, // No material
			defaultMeshPrimitiveParams);
	}


	// TODO: Most of the meshfactory functions are still hard-coded for OpenGL spaces
	inline std::shared_ptr<MeshPrimitive> CreateQuad(
		glm::vec3 tl /*= vec3(-0.5f, 0.5f, 0.0f)*/,
		glm::vec3 tr /*= vec3(0.5f, 0.5f, 0.0f)*/,
		glm::vec3 bl /*= vec3(-0.5f, -0.5f, 0.0f)*/,
		glm::vec3 br /*= vec3(0.5f, -0.5f, 0.0f)*/)
	{
		std::vector<glm::vec3> positions = { tl, bl, tr, br };

		std::vector<glm::vec2> uvs // Note: (0,0) = Top left
		{
			glm::vec2(0.f, 0.f), // tl
			glm::vec2(0.f, 1.f), // bl
			glm::vec2(1.f, 0.f), // tr
			glm::vec2(1.f, 1.f)  // br
		};

		std::vector<uint32_t> quadIndices
		{
			0, 1, 2,	// TL face
			2, 1, 3		// BR face
		}; // Note: CCW winding

		std::vector<glm::vec4> colors(4, glm::vec4(1, 0, 0, 1)); // Assign a bright red color by default...

		constexpr char meshName[] = "quad";

		const MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams;
		util::VertexStreamBuilder::MeshData meshData
		{
			meshName,
			&defaultMeshPrimitiveParams,
			&quadIndices,
			&positions,
			nullptr,
			nullptr,
			&uvs,
			&colors,
			nullptr,
			nullptr };
		util::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

		// It's easier to reason about geometry in vecN types; cast to float now we're done
		return MeshPrimitive::Create(
			meshName,
			&quadIndices,
			*reinterpret_cast<std::vector<float>*>(&positions), // Cast our vector<glm::vec3> to vector<float>
			gr::Bounds::k_invalidMinXYZ,
			gr::Bounds::k_invalidMaxXYZ,
			nullptr,
			nullptr,
			reinterpret_cast<std::vector<float>*>(&uvs),
			reinterpret_cast<std::vector<float>*>(&colors),
			nullptr, // No joints
			nullptr, // No weights
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
		std::vector<glm::vec3> positions(numVerts);
		std::vector<glm::vec3> normals(numVerts);
		std::vector<glm::vec2> uvs(numVerts);

		const size_t numIndices = 3 * numLatSlices * numLongSlices * 2;
		std::vector<uint32_t> indices(numIndices);

		// Generate a sphere about the Y axis:
		glm::vec3 firstPosition = glm::vec3(0.0f, radius, 0.0f);
		glm::vec3 firstNormal = glm::vec3(0, 1.0f, 0);
		glm::vec3 firstTangent = glm::vec3(0, 0, 0);
		glm::vec2 firstUv0 = glm::vec2(0.5f, 0.0f);

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

				glm::vec3 position = glm::vec3(x, y, z);
				glm::vec3 normal = glm::normalize(position);
				glm::vec2 uv0 = glm::vec2(uvX, uvY);

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
		const glm::vec3 finalPosition = glm::vec3(0.0f, -radius, 0.0f);
		const glm::vec3 finalNormal = glm::vec3(0, -1, 0);
		const glm::vec2 finalUv0 = glm::vec2(0.5f, 1.0f);

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

		constexpr char meshName[] = "sphere";

		const MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams;
		util::VertexStreamBuilder::MeshData meshData
		{
			meshName,
			&defaultMeshPrimitiveParams,
			&indices,
			&positions,
			&normals,
			nullptr,
			&uvs,
			nullptr,
			nullptr,
			nullptr };
		util::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

		// Legacy: Previously, we stored vertex data in vecN types. Instead of rewriting, just cast to float
		return MeshPrimitive::Create(
			meshName,
			&indices,
			*reinterpret_cast<std::vector<float>*>(&positions), // Cast our vector<glm::vec3> to vector<float>
			gr::Bounds::k_invalidMinXYZ,
			gr::Bounds::k_invalidMaxXYZ,
			reinterpret_cast<std::vector<float>*>(&normals),
			nullptr,
			reinterpret_cast<std::vector<float>*>(&uvs),
			nullptr,
			nullptr, // No joints
			nullptr, // No weights
			nullptr, // No material
			defaultMeshPrimitiveParams);
	}


	inline std::shared_ptr<gr::MeshPrimitive> CreateHelloTriangle(float scale /*= 1.f*/, float zDepth /*= 0.5f*/)
	{
		std::vector<glm::vec3> positions // In clip space: bl near = [-1,-1, 0] , tr far = [1,1,1]
		{
			glm::vec3(0.0f * scale,		0.75f * scale,	zDepth),	// Top center
			glm::vec3(-0.75f * scale,	-0.75f * scale, zDepth),	// bl
			glm::vec3(0.75f * scale,		-0.75f * scale, zDepth)		// br
		};

		std::vector<glm::vec2> uvs // Note: (0,0) = Top left
		{
			glm::vec2(0.5f, 0.f),	// Top center
			glm::vec2(0.f, 1.f),		// bl
			glm::vec2(1.f, 0.f),		// br
		};

		std::vector<uint32_t> indices
		{
			0, 1, 2
		}; // Note: CCW winding

		std::vector<glm::vec4> colors
		{
			glm::vec4(1.f, 0.f, 0.f, 1.f), // Top center: Red
			glm::vec4(0.f, 1.f, 0.f, 1.f), // bl: Green
			glm::vec4(0.f, 0.f, 1.f, 1.f), // br: Blue
		};

		constexpr char meshName[] = "helloTriangle";

		const MeshPrimitive::MeshPrimitiveParams defaultMeshPrimitiveParams;
		util::VertexStreamBuilder::MeshData meshData
		{
			meshName,
			&defaultMeshPrimitiveParams,
			&indices,
			&positions,
			nullptr,
			nullptr,
			&uvs,
			&colors,
			nullptr,
			nullptr
		};
		util::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);

		std::shared_ptr<gr::Material> helloMaterial =
			gr::Material::Create("HelloTriangleMaterial", gr::Material::MaterialType::GLTF_PBRMetallicRoughness);

		// It's easier to reason about geometry in vecN types; cast to float now we're done
		return MeshPrimitive::Create(
			meshName,
			&indices,
			*reinterpret_cast<std::vector<float>*>(&positions), // Cast our vector<glm::vec3> to vector<float>
			gr::Bounds::k_invalidMinXYZ,
			gr::Bounds::k_invalidMaxXYZ,
			nullptr,
			nullptr,
			reinterpret_cast<std::vector<float>*>(&uvs),
			reinterpret_cast<std::vector<float>*>(&colors),
			nullptr, // No joints
			nullptr, // No weights
			helloMaterial,
			MeshPrimitive::MeshPrimitiveParams());
	}
}