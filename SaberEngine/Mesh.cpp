#include <memory>
#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "Mesh.h"
#include "CoreEngine.h"

using gr::Transform;
using glm::pi;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using std::move;
using std::vector;
using std::string;
using std::shared_ptr;


namespace gr
{
	// Returns a Bounds, transformed from local space using worldMatrix
	Bounds Bounds::GetTransformedBounds(mat4 const& worldMatrix)
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
			points[i] = worldMatrix * points[i];

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

	Mesh::Mesh(
		string const& name,
		vector<float>& positions,
		vector<float>& normals,
		vector<float>& colors,
		vector<float>& uv0,
		vector<float>& tangents,
		vector<uint32_t>& indices,
		shared_ptr<gr::Material> material,
		MeshParams const& meshParams,
		Transform* ownerTransform) : NamedObject(name),
			m_platformParams(nullptr),
			m_meshMaterial(material),
			m_params(meshParams),
			m_ownerTransform(ownerTransform)
	{
		m_positions		= move(positions);
		m_normals		= move(normals);
		m_colors		= move(colors);
		m_uv0			= move(uv0);
		m_tangents		= move(tangents);
		m_indices		= move(indices);

		ComputeBounds(); // Compute m_localBounds

		platform::Mesh::Create(*this); // Platform-specific setup

		ComputeDataHash();
	}


	void Mesh::Destroy()
	{
		m_positions.clear();
		m_normals.clear();
		m_colors.clear();
		m_uv0.clear();
		m_tangents.clear();
		m_indices.clear();

		m_meshMaterial = nullptr;

		platform::Mesh::Destroy(*this); // Platform-specific destruction
		m_platformParams = nullptr;
	}


	void Mesh::Bind(bool doBind) const
	{
		platform::Mesh::Bind(m_platformParams.get(), doBind);
	}


	void Mesh::ComputeBounds()
	{
		for (size_t i = 0; i < m_positions.size(); i+=3) // Stride of 3
		{
			// Legacy: Previously, we stored vertex data in vecN types. Instead of rewriting, just cast to float
			vec3 const& posVector = reinterpret_cast<vec3 const&>(m_positions[i]);
			if (posVector.x < m_localBounds.xMin())
			{
				m_localBounds.xMin() = posVector.x;
			}
			if (posVector.x > m_localBounds.xMax())
			{
				m_localBounds.xMax() = posVector.x;
			}

			if (posVector.y < m_localBounds.yMin())
			{
				m_localBounds.yMin() = posVector.y;
			}
			if (posVector.y > m_localBounds.yMax())
			{
				m_localBounds.yMax() = posVector.y;
			}

			if (posVector.z < m_localBounds.zMin())
			{
				m_localBounds.zMin() = posVector.z;
			}
			if (posVector.z > m_localBounds.zMax())
			{
				m_localBounds.zMax() = posVector.z;
			}
		}
	}


	void Mesh::ComputeDataHash()
	{
		// Material:
		if (m_meshMaterial)
		{
			AddDataBytesToHash(&m_meshMaterial->GetName()[0], m_meshMaterial->GetName().length() * sizeof(char));
		}

		// Mesh params:
		AddDataBytesToHash(&m_params, sizeof(MeshParams));

		// Vertex data streams:
		AddDataBytesToHash(&m_positions[0], sizeof(float) * m_positions.size());

		if (!m_normals.empty())
		{
			AddDataBytesToHash(&m_normals[0], sizeof(float) * m_normals.size());
		}
		if (!m_colors.empty())
		{
			AddDataBytesToHash(&m_colors[0], sizeof(float) * m_colors.size());
		}
		if (!m_uv0.empty())
		{
			AddDataBytesToHash(&m_uv0[0], sizeof(float) * m_uv0.size());
		}
		if (!m_tangents.empty())
		{
			AddDataBytesToHash(&m_tangents[0], sizeof(float) * m_tangents.size());
		}
		if (!m_indices.empty())
		{
			AddDataBytesToHash(&m_indices[0], sizeof(uint32_t) * m_indices.size());
		}		
	}


	namespace meshfactory
	{
		inline std::shared_ptr<Mesh> CreateCube()
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

			const vector<vec3 > normals
			{
				vec3(0.0f, 0.0f, 1.0f),		// Front = 0
				vec3(0.0f, 0.0f, -1.0f),	// Back = 1
				vec3(-1.0f, 0.0f, 0.0f),	// Left = 2
				vec3(1.0f, 0.0f, 0.0f),		// Right = 3
				vec3(0.0f, 1.0f, 0.0f),		// Up = 4
				vec3(0.0f, -1.0f, 0.0f),	// Down = 5
			};

			const vector<vec4> colors
			{
				vec4(0.0f, 0.0f, 0.0f, 1.0f),
				vec4(0.0f, 0.0f, 1.0f, 1.0f),
				vec4(0.0f, 1.0f, 0.0f, 1.0f),
				vec4(0.0f, 1.0f, 1.0f, 1.0f),
				vec4(1.0f, 0.0f, 0.0f, 1.0f),
				vec4(1.0f, 0.0f, 1.0f, 1.0f),
				vec4(1.0f, 1.0f, 0.0f, 1.0f),
				vec4(1.0f, 1.0f, 1.0f, 1.0f),
			};

			const vector<vec2> uvs
			{
				vec2(0.0f, 0.0f),
				vec2(0.0f, 1.0f),
				vec2(1.0f, 0.0f),
				vec2(1.0f, 1.0f),
			};

			// Assemble the vertex data into streams.
			// Debugging hint: position index should = color index. All UV's should be used once per face.		
			vector<vec3> assembledPositions
			{
				// Front face
				positions[0], positions[1], positions[2], positions[3],
				
				// Left face
				positions[4], positions[5],	positions[1], positions[0],

				// Right face
				positions[3], positions[2], positions[6], positions[7],

				// Top face
				positions[4], positions[0], positions[3], positions[7],

				// Bottom face
				positions[1], positions[5],	positions[6], positions[2],

				// Back face
				positions[7], positions[6], positions[5], positions[4]
			};

			vector<vec3> assembledNormals
			{
				// Front face
				normals[0], normals[0], normals[0],	normals[0],

				// Left face
				normals[2], normals[2],	normals[2],	normals[2],

				// Right face
				normals[3], normals[3],	normals[3],	normals[3],

				// Top face
				normals[4], normals[4], normals[4],	normals[4],

				// Bottom face
				normals[5], normals[5],	normals[5],	normals[5],

				// Back face
				normals[1], normals[1],	normals[1],	normals[1]
			};

			vector<vec4> assembledColors
			{
				// Front face
				colors[0], colors[1], colors[2], colors[3],

				// Left face
				colors[4], colors[5], colors[1], colors[0],

				// Right face
				colors[3], colors[2], colors[6], colors[7],

				// Top face
				colors[4], colors[0], colors[3], colors[7],

				// Bottom face
				colors[1], colors[5], colors[6], colors[2],

				// Back face
				colors[7], colors[6], colors[5], colors[4]
			};

			vector<vec2> assembledUVs
			{
				// Front face
				uvs[1], uvs[0], uvs[2],	uvs[3],
					  
				// Left face
				uvs[1], uvs[0],	uvs[2],	uvs[3],
					  
				// Right face
				uvs[1], uvs[0],	uvs[2],	uvs[3],
					  
				// Top face
				uvs[1], uvs[0],	uvs[2],	uvs[3],
					  
				// Bottom face
				uvs[1], uvs[0],	uvs[2],	uvs[3],
					  
				// Back face
				uvs[1], uvs[0],	uvs[2],	uvs[3]
			};

			// TODO: Populate these with meaningful data
			vector<vec3> assembledTangents(positions.size());

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

			
			// Legacy: Previously, we stored vertex data in vecN types. Instead of rewriting, just cast to float
			return std::make_shared<Mesh>(
				"cube", 
				*reinterpret_cast<vector<float>*>(&assembledPositions),	// Cast our vector<vec3> to vector<float>
				*reinterpret_cast<vector<float>*>(&assembledNormals),
				*reinterpret_cast<vector<float>*>(&assembledColors),
				*reinterpret_cast<vector<float>*>(&assembledUVs),
				*reinterpret_cast<vector<float>*>(&assembledTangents),
				cubeIndices, 
				nullptr,
				Mesh::MeshParams(),
				nullptr);
		}


		inline std::shared_ptr<Mesh> CreateFullscreenQuad(bool onNearPlane) // On far plane by default
		{
			float zDepth;
			switch (en::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI())
			{
			case platform::RenderingAPI::OpenGL:
			{
				if (onNearPlane) zDepth = -1.0f;
				else zDepth = 1.f;
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

			#if 0
			return CreateQuad(nullptr,
				vec3(-1.0f, 1.0f,	zDepth),	// TL
				vec3(1.0f,	1.0f,	zDepth),	// TR
				vec3(-1.0f, -1.0f,	zDepth),	// BL
				vec3(1.0f,	-1.0f,	zDepth));	// BR
			#endif

			// Create a triangle twice the size of clip space, and let the clipping hardware trim it to size:
			std::vector<vec2> uvs
			{
				vec2(0.f, 2.f), // tl
				vec2(0.f, 0.f), // bl
				vec2(2.f, 0.f)  // br
			};

			const vec3 tl = vec3(-1.f, 3.f, zDepth);
			const vec3 bl = vec3(-1.f, -1.f, zDepth);
			const vec3 br = vec3(3.0f, -1.0f, zDepth);

			std::vector<vec3> positions = {tl, bl, br};
			const vec3 tangent = normalize(vec3(br - bl));
			const vec3 bitangent = normalize(vec3(tl - bl));
			const vec3 normal = normalize(cross(tangent, bitangent));
			const vec4 redColor = vec4(1, 0, 0, 1); // Assign a bright red color by default

			std::vector<vec3> normals(3, normal);
			std::vector<vec4> colors(3, redColor);
			std::vector<vec3> tangents(3, tangent); // TODO: Populate this

			std::vector<uint32_t> triIndices {0, 1, 2}; // Note: CCW winding
			
			return std::make_shared<Mesh>(
				"optimizedFullscreenQuad",
				*reinterpret_cast<vector<float>*>(&positions), // Cast our vector<vec3> to vector<float>
				*reinterpret_cast<vector<float>*>(&normals),
				*reinterpret_cast<vector<float>*>(&colors),
				*reinterpret_cast<vector<float>*>(&uvs),
				*reinterpret_cast<vector<float>*>(&tangents),
				triIndices,
				nullptr,
				Mesh::MeshParams(),
				nullptr);
		}


		// TODO: Most of the meshfactory functions are still hard-coded for OpenGL spaces
		inline std::shared_ptr<Mesh> CreateQuad(
			vec3 tl /*= vec3(-0.5f, 0.5f, 0.0f)*/,
			vec3 tr /*= vec3(0.5f, 0.5f, 0.0f)*/,
			vec3 bl /*= vec3(-0.5f, -0.5f, 0.0f)*/,
			vec3 br /*= vec3(0.5f, -0.5f, 0.0f)*/)
		{
			vec3 tangent = normalize(vec3(br - bl));
			vec3 bitangent = normalize(vec3(tl - bl));
			vec3 quadNormal = normalize(cross(tangent, bitangent));
			vec4 redColor = vec4(1, 0, 0, 1); // Assign a bright red color by default...

			std::vector<vec2> uvs
			{
				vec2(0, 1), // tl
				vec2(0, 0), // bl
				vec2(1, 1), // tr
				vec2(1, 0)  // br
			};

			std::vector<uint32_t> quadIndices
			{
				0, 1, 2,	// TL face
				2, 1, 3		// BR face
			}; // Note: CCW winding

			// Assemble the vertex data streams:
			std::vector<vec3> positions = {tl, bl, tr, br};
			std::vector<vec3> normals(4, quadNormal);
			std::vector<vec4> colors(4, redColor);
			std::vector<vec3> tangents(positions.size()); // TODO: Populate this

			// It's easier to reason about geometry in vecN types; cast to float now we're done
			return std::make_shared<Mesh>(
				"quad", 
				*reinterpret_cast<vector<float>*>(&positions), // Cast our vector<vec3> to vector<float>
				*reinterpret_cast<vector<float>*>(&normals),
				*reinterpret_cast<vector<float>*>(&colors),
				*reinterpret_cast<vector<float>*>(&uvs),
				*reinterpret_cast<vector<float>*>(&tangents),
				quadIndices, 
				nullptr,
				Mesh::MeshParams(),
				nullptr);
		}


		inline std::shared_ptr<Mesh> CreateSphere(
			float radius /*= 0.5f*/,
			size_t numLatSlices /*= 16*/,
			size_t numLongSlices /*= 16*/)
		{
			// NOTE: Currently, this function does not generate valid tangents for any verts. Some UV's are distorted,
			// as we're using merged vertices. TODO: Fix this

			// Note: Latitude = horizontal lines about Y
			//		Longitude = vertical lines about sphere
			//		numLatSlices = horizontal segments
			//		numLongSlices = vertical segments

			const size_t numVerts = numLatSlices * numLongSlices + 2; // + 2 for end caps

			vector<vec3> positions(numVerts);
			vector<vec3> normals(numVerts);
			vector<vec4> colors(numVerts);
			vector<vec2> uvs(numVerts);
			vector<vec3> tangents(numVerts);

			// TODO: Actually compute these. For now, just use dummy values
			const vec4 vertColor(1.0f, 1.0f, 1.0f, 1.0f);


			const size_t numIndices = 3 * numLatSlices * numLongSlices * 2;
			vector<uint32_t> indices(numIndices);

			// Generate a sphere about the Y axis:
			vec3 firstPosition = vec3(0.0f, radius, 0.0f);
			vec3 firstNormal = vec3(0, 1.0f, 0);
			vec3 firstTangent = vec3(0, 0, 0);
			vec2 firstUv0 = vec2(0.5f, 1.0f);

			size_t currentIndex = 0;

			positions[currentIndex] = firstPosition;
			normals[currentIndex] = firstNormal;
			colors[currentIndex] = vertColor;
			uvs[currentIndex] = firstUv0;
			tangents[currentIndex] = firstTangent;
			currentIndex++;

			// Rotate about Z: Arc down the side profile of our sphere
			// cos theta = adj/hyp -> hyp * cos theta = adj -> radius * cos theta = Y
			float zRadianStep = pi<float>() / (float)(numLongSlices + 1); // +1 to get the number of rows
			float zRadians = zRadianStep; // Already added cap vertex, so start on the next step

			// Rotate about Y: Horizontal edges
			// sin theta = opp/hyp -> hyp * sin theta = opp -> radius * sin theta = X
			// cos theta = adj/hyp -> hyp * cos theta = adj -> radius * cos theta = Z
			float yRadianStep = (2.0f * pi<float>()) / (float)numLatSlices; //
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

					vec3 position = vec3(x, y, z);
					vec3 normal = normalize(position);

					vec3 tangent = vec3(1, 0, 0); // TODO: Compute this
					vec2 uv0 = vec2(uvX, uvY);

					positions[currentIndex] = position;
					normals[currentIndex] = normal;
					colors[currentIndex] = vertColor;
					uvs[currentIndex] = uv0;
					tangents[currentIndex] = tangent;
					currentIndex++;

					uvX += uvXStep;
				}
				yRadians = 0.0f;
				zRadians += zRadianStep;

				uvX = 0;
				uvY -= uvYStep;
			}

			// Final endcap:
			vec3 finalPosition = vec3(0.0f, -radius, 0.0f);
			vec3 finalNormal = vec3(0, -1, 0);

			vec3 finalTangent = vec3(0, 0, 0);
			vec3 finalBitangent = vec3(0, 0, 0);
			vec2 finalUv0 = vec2(0.5f, 0.0f);

			positions[currentIndex]		= finalPosition;
			normals[currentIndex]		= finalNormal;
			colors[currentIndex]		= vertColor;
			uvs[currentIndex]			= finalUv0;
			tangents[currentIndex]		= finalTangent;

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

			// Legacy: Previously, we stored vertex data in vecN types. Instead of rewriting, just cast to float
			return make_shared<Mesh>(
				"sphere", 
				*reinterpret_cast<vector<float>*>(&positions), // Cast our vector<vec3> to vector<float>
				*reinterpret_cast<vector<float>*>(&normals),
				*reinterpret_cast<vector<float>*>(&colors),
				*reinterpret_cast<vector<float>*>(&uvs),
				*reinterpret_cast<vector<float>*>(&tangents),
				indices, 
				nullptr,
				Mesh::MeshParams(),
				nullptr);
		}
	} // meshfactory
} // gr


