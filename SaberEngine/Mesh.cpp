#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
using glm::pi;

#include "DebugConfiguration.h"
#include "Mesh.h"


namespace gr
{
	// Returns a Bounds, transformed from local space using transform
	Bounds Bounds::GetTransformedBounds(mat4 const& m_transform)
	{
		// Temp: Ensure the bounds are 3D here, before we do any calculations
		Make3Dimensional();

		Bounds result;

		std::vector<vec4>points(8);											// "front" == fwd == Z -
		points[0] = vec4(m_xMin, m_yMax, m_zMin, 1.0f);		// Left		top		front 
		points[1] = vec4(m_xMax, m_yMax, m_zMin, 1.0f);		// Right	top		front
		points[2] = vec4(m_xMin, m_yMin, m_zMin, 1.0f);		// Left		bot		front
		points[3] = vec4(m_xMax, m_yMin, m_zMin, 1.0f);		// Right	bot		front

		points[4] = vec4(m_xMin, m_yMax, m_zMax, 1.0f);		// Left		top		back
		points[5] = vec4(m_xMax, m_yMax, m_zMax, 1.0f);		// Right	top		back
		points[6] = vec4(m_xMin, m_yMin, m_zMax, 1.0f);		// Left		bot		back
		points[7] = vec4(m_xMax, m_yMin, m_zMax, 1.0f);		// Right	bot		back

		for (size_t i = 0; i < 8; i++)
		{
			points[i] = m_transform * points[i];

			if (points[i].x < result.m_xMin)
			{
				result.m_xMin = points[i].x;
			}
			if (points[i].x > result.m_xMax)
			{
				result.m_xMax = points[i].x;
			}

			if (points[i].y < result.m_yMin)
			{
				result.m_yMin = points[i].y;
			}
			if (points[i].y > result.m_yMax)
			{
				result.m_yMax = points[i].y;
			}

			if (points[i].z < result.m_zMin)
			{
				result.m_zMin = points[i].z;
			}
			if (points[i].z > result.m_zMax)
			{
				result.m_zMax = points[i].z;
			}
		}

		return result;
	}


	void Bounds::Make3Dimensional()
	{
		float depthBias = 0.01f;
		if (glm::abs(m_xMax - m_xMin) < depthBias)
		{
			m_xMax += depthBias;
			m_xMin -= depthBias;
		}

		if (glm::abs(m_yMax - m_yMin) < depthBias)
		{
			m_yMax += depthBias;
			m_yMin -= depthBias;
		}

		if (glm::abs(m_zMax - m_zMin) < depthBias)
		{
			m_zMax += depthBias;
			m_zMin -= depthBias;
		}
	}


	/******************************************************************************************************************/


	Mesh::Mesh(std::string name, 
		std::vector<Vertex> vertices, 
		std::vector<uint32_t> indices, 
		std::shared_ptr<gr::Material> newMeshMaterial) :
			m_platformParams{ platform::Mesh::PlatformParams::CreatePlatformParams() },
			meshName{ name },
			m_vertices{ vertices },
			m_indices{ indices },
			m_meshMaterial{ newMeshMaterial }
	{
		// Compute the localBounds:
		ComputeBounds();

		// Platform-specific setup:
		platform::Mesh::Create(*this);
	}


	void Mesh::Destroy()
	{
		#if defined(DEBUG_LOG_OUTPUT)
			meshName = meshName + "_DESTROYED"; // Safety...
		#endif

		if (m_vertices.size() > 0)
		{
			m_vertices.clear();
		}
		if (m_indices.size() > 0)
		{
			m_indices.clear();
		}

		m_meshMaterial = nullptr;		// Note: Material MUST be cleaned up elsewhere!

		// Platform-specific destruction:
		platform::Mesh::Destroy(*this);

		m_platformParams = nullptr;
	}


	void Mesh::Bind(bool doBind)
	{
		platform::Mesh::Bind(*this, doBind);
	}


	void Mesh::ComputeBounds()
	{
		for (size_t i = 0; i < m_vertices.size(); i++)
		{
			if (m_vertices[i].m_position.x < m_localBounds.xMin())
			{
				m_localBounds.xMin() = m_vertices[i].m_position.x;
			}
			if (m_vertices[i].m_position.x > m_localBounds.xMax())
			{
				m_localBounds.xMax() = m_vertices[i].m_position.x;
			}

			if (m_vertices[i].m_position.y < m_localBounds.yMin())
			{
				m_localBounds.yMin() = m_vertices[i].m_position.y;
			}
			if (m_vertices[i].m_position.y > m_localBounds.yMax())
			{
				m_localBounds.yMax() = m_vertices[i].m_position.y;
			}

			if (m_vertices[i].m_position.z < m_localBounds.zMin())
			{
				m_localBounds.zMin() = m_vertices[i].m_position.z;
			}
			if (m_vertices[i].m_position.z > m_localBounds.zMax())
			{
				m_localBounds.zMax() = m_vertices[i].m_position.z;
			}
		}
	}


	namespace meshfactory
	{
		inline std::shared_ptr<Mesh> CreateCube(std::shared_ptr<gr::Material> newMeshMaterial /*= nullptr*/)
		{
			// Note: SaberEngine uses a RHCS in all cases
			std::vector<vec3> positions(8);
			positions[0] = glm::vec3(-1.0f, 1.0f, 1.0f); // "Front" side
			positions[1] = glm::vec3(-1.0f, -1.0f, 1.0f);
			positions[2] = glm::vec3(1.0f, -1.0f, 1.0f);
			positions[3] = glm::vec3(1.0f, 1.0f, 1.0f);

			positions[4] = glm::vec3(-1.0f, 1.0f, -1.0f); // "Back" side
			positions[5] = glm::vec3(-1.0f, -1.0f, -1.0f);
			positions[6] = glm::vec3(1.0f, -1.0f, -1.0f);
			positions[7] = glm::vec3(1.0f, 1.0f, -1.0f);

			const std::vector<glm::vec3 > normals
			{
				glm::vec3(0.0f, 0.0f, 1.0f),	// Front = 0
				glm::vec3(0.0f, 0.0f, -1.0f),	// Back = 1
				glm::vec3(-1.0f, 0.0f, 0.0f),	// Left = 2
				glm::vec3(1.0f, 0.0f, 0.0f),	// Right = 3
				glm::vec3(0.0f, 1.0f, 0.0f),	// Up = 4
				glm::vec3(0.0f, -1.0f, 0.0f),	// Down = 5
			};

			const std::vector<glm::vec4> colors
			{
				glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
				glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
				glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
				glm::vec4(0.0f, 1.0f, 1.0f, 1.0f),
				glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
				glm::vec4(1.0f, 0.0f, 1.0f, 1.0f),
				glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
				glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
			};

			const std::vector<glm::vec4> uvs
			{
				glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
				glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
				glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
				glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
			};

			std::vector<gr::Vertex> cubeVerts
			{
				// TODO: Implement hard-coded tangent & bitangents instead of empty vec3's...

				// Front face:
				Vertex(positions[0], normals[0], glm::vec3(), glm::vec3(), colors[0], uvs[1]), // HINT: position index should = color index
				Vertex(positions[1], normals[0], glm::vec3(), glm::vec3(), colors[1], uvs[0]), // All UV's should be used once per face
				Vertex(positions[2], normals[0], glm::vec3(), glm::vec3(), colors[2], uvs[2]), //2
				Vertex(positions[3], normals[0], glm::vec3(), glm::vec3(), colors[3], uvs[3]), //3

				// Left face:
				Vertex(positions[4], normals[2], glm::vec3(), glm::vec3(), colors[4], uvs[1]), //4
				Vertex(positions[5], normals[2], glm::vec3(), glm::vec3(), colors[5], uvs[0]),
				Vertex(positions[1], normals[2], glm::vec3(), glm::vec3(), colors[1], uvs[2]),
				Vertex(positions[0], normals[2], glm::vec3(), glm::vec3(), colors[0], uvs[3]), //7

				// Right face:
				Vertex(positions[3], normals[3], glm::vec3(), glm::vec3(), colors[3], uvs[1]), //8
				Vertex(positions[2], normals[3], glm::vec3(), glm::vec3(), colors[2], uvs[0]),
				Vertex(positions[6], normals[3], glm::vec3(), glm::vec3(), colors[6], uvs[2]),
				Vertex(positions[7], normals[3], glm::vec3(), glm::vec3(), colors[7], uvs[3]), //11

				// Top face:
				Vertex(positions[4], normals[4], glm::vec3(), glm::vec3(), colors[4], uvs[1]), //12
				Vertex(positions[0], normals[4], glm::vec3(), glm::vec3(), colors[0], uvs[0]),
				Vertex(positions[3], normals[4], glm::vec3(), glm::vec3(), colors[3], uvs[2]),
				Vertex(positions[7], normals[4], glm::vec3(), glm::vec3(), colors[7], uvs[3]), //15

				// Bottom face:
				Vertex(positions[1], normals[5], glm::vec3(), glm::vec3(), colors[1], uvs[1]), //16
				Vertex(positions[5], normals[5], glm::vec3(), glm::vec3(), colors[5], uvs[0]),
				Vertex(positions[6], normals[5], glm::vec3(), glm::vec3(), colors[6], uvs[2]),
				Vertex(positions[2], normals[5], glm::vec3(), glm::vec3(), colors[2], uvs[3]), //19

				// Back face:
				Vertex(positions[7], normals[1], glm::vec3(), glm::vec3(), colors[7], uvs[1]), //20
				Vertex(positions[6], normals[1], glm::vec3(), glm::vec3(), colors[6], uvs[0]),
				Vertex(positions[5], normals[1], glm::vec3(), glm::vec3(), colors[5], uvs[2]),
				Vertex(positions[4], normals[1], glm::vec3(), glm::vec3(), colors[4], uvs[3]), //23
			};

			std::vector<uint32_t> cubeIndices // 6 faces * 2 tris * 3 indices 
			{
				// Front face:
				0, 1, 3,
				1, 2, 3,

				// Left face:
				4, 5, 7,
				7, 5, 6,

				// Right face:
				8, 9, 11,
				9, 10, 11,

				// Top face:
				12, 13, 15,
				13, 14, 15,

				// Bottom face:
				16, 17, 19,
				17, 18, 19,

				// Back face:
				20, 21, 23,
				21, 22, 23,
			};

			return std::make_shared<Mesh>("cube", cubeVerts, cubeIndices, newMeshMaterial);
		}

		inline std::shared_ptr<Mesh> CreateQuad(glm::vec3 tl /*= vec3(-0.5f, 0.5f, 0.0f)*/,
			glm::vec3 tr /*= vec3(0.5f, 0.5f, 0.0f)*/,
			glm::vec3 bl /*= vec3(-0.5f, -0.5f, 0.0f)*/,
			glm::vec3 br /*= vec3(0.5f, -0.5f, 0.0f)*/,
			std::shared_ptr<gr::Material> newMeshMaterial /*= nullptr*/)
		{
			glm::vec3 m_tangent = normalize(vec3(br - bl));
			glm::vec3 m_bitangent = normalize(vec3(tl - bl));
			glm::vec3 quadNormal = normalize(cross(m_tangent, m_bitangent));
			glm::vec4 redColor = vec4(1, 0, 0, 1); // Assign a bright red color by default...

			std::vector<glm::vec4> uvs
			{
				glm::vec4(0, 1, 0, 0), // tl
				glm::vec4(0, 0, 0, 0), // bl
				glm::vec4(1, 1, 0, 0), // tr
				glm::vec4(1, 0, 0, 0)  // br
			};

			std::vector<Vertex> quadVerts
			{
				// tl
				Vertex(tl, quadNormal, m_tangent, m_bitangent, redColor, uvs[0]),

				// bl
				Vertex(bl, quadNormal, m_tangent, m_bitangent, redColor, uvs[1]),

				// tr
				Vertex(tr, quadNormal, m_tangent, m_bitangent, redColor, uvs[2]),

				// br
				Vertex(br, quadNormal, m_tangent, m_bitangent, redColor, uvs[3])
			};

			std::vector<uint32_t> quadIndices
			{
				// TL face:
				0, 1, 2,
				// BR face:
				2, 1, 3
			}; // Note: CCW winding

			return std::make_shared<Mesh>("quad", quadVerts, quadIndices, newMeshMaterial);
		}


		inline std::shared_ptr<Mesh> CreateSphere(float radius /*= 0.5f*/,
			size_t numLatSlices /*= 16*/,
			size_t numLongSlices /*= 16*/,
			std::shared_ptr<gr::Material> newMeshMaterial /*= nullptr*/)
		{
			// NOTE: Currently, this function does not generate valid tangents for any verts. Some UV's are distorted,
			// as we're using merged vertices. TODO: Fix this

			// Note: Latitude = horizontal lines about Y
			//		Longitude = vertical lines about sphere
			//		numLatSlices = horizontal segments
			//		numLongSlices = vertical segments

			size_t numVerts = numLatSlices * numLongSlices + 2; // + 2 for end caps
			std::vector<Vertex> vertices(numVerts);
			std::vector<vec3>normals(numVerts);
			std::vector<vec4>uvs(numVerts);

			glm::vec4 vertColor(1.0f, 1.0f, 1.0f, 1.0f);

			size_t numIndices = 3 * numLatSlices * numLongSlices * 2;
			std::vector<uint32_t> indices(numIndices);

			// Generate a sphere about the Y axis:
			glm::vec3 firstPosition = glm::vec3(0.0f, radius, 0.0f);
			glm::vec3 firstNormal = glm::vec3(0, 1.0f, 0);
			glm::vec3 firstTangent = glm::vec3(0, 0, 0); //
			glm::vec3 firstBitangent = glm::vec3(0, 0, 0); //
			glm::vec4 firstUv0 = glm::vec4(0.5f, 1.0f, 0, 0);

			size_t currentIndex = 0;
			vertices[currentIndex++] =
				Vertex(firstPosition, firstNormal, firstTangent, firstBitangent, vertColor, firstUv0);

			// Rotate about Z: Arc down the side profile of our sphere
			// cos theta = adj/hyp -> hyp * cos theta = adj -> radius * cos theta = Y
			float zRadianStep = glm::pi<float>() / (float)(numLongSlices + 1); // +1 to get the number of rows
			float zRadians = zRadianStep; // Already added cap vertex, so start on the next step

			// Rotate about Y: Horizontal edges
			// sin theta = opp/hyp -> hyp * sin theta = opp -> radius * sin theta = X
			// cos theta = adj/hyp -> hyp * cos theta = adj -> radius * cos theta = Z
			float yRadianStep = (2.0f * glm::pi<float>()) / (float)numLatSlices; //
			float yRadians = 0.0f;

			// Build UV's, from top left (0,1) to bottom right (1.0, 0)
			float uvXStep = 1.0f / (float)numLatSlices;
			float uvYStep = 1.0f / (float)(numLongSlices + 1);
			float uvX = 0;
			float uvY = 1.0f - uvYStep;

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

					glm::vec3 m_position = glm::vec3(x, y, z);
					glm::vec3 m_normal = normalize(m_position);

					glm::vec3 m_tangent = glm::vec3(1, 0, 0); // TODO
					glm::vec3 m_bitangent = glm::vec3(0, 1, 0); // TODO
					glm::vec4 m_uv0 = glm::vec4(uvX, uvY, 0, 0);

					vertices[currentIndex++] = Vertex(m_position, m_normal, m_tangent, m_bitangent, vertColor, m_uv0);

					uvX += uvXStep;
				}
				yRadians = 0.0f;
				zRadians += zRadianStep;

				uvX = 0;
				uvY -= uvYStep;
			}

			// Final endcap:
			glm::vec3 finalPosition = glm::vec3(0.0f, -radius, 0.0f);
			glm::vec3 finalNormal = glm::vec3(0, -1, 0);

			glm::vec3 finalTangent = glm::vec3(0, 0, 0);
			glm::vec3 finalBitangent = glm::vec3(0, 0, 0);
			glm::vec4 finalUv0 = glm::vec4(0.5f, 0.0f, 0, 0);

			vertices[currentIndex] =
				Vertex(finalPosition, finalNormal, finalTangent, finalBitangent, vertColor, finalUv0);

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

			return std::make_shared<Mesh>("sphere", vertices, indices, newMeshMaterial);
		}
	} // meshfactory
} // gr


