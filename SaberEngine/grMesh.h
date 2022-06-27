#pragma once

#include <string>
#include <memory>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "Transform.h"
#include "Material.h"


using SaberEngine::Transform; // TODO: Delete this


namespace gr
{
	enum VERTEX_BUFFER_OBJECT
	{
		BUFFER_VERTICES,
		BUFFER_INDEXES,

		BUFFER_COUNT, // Reserved: Number of buffers to allocate
	};


	enum VERTEX_ATTRIBUTE
	{
		VERTEX_POSITION = 0,
		VERTEX_COLOR = 1,

		VERTEX_NORMAL = 2,
		VERTEX_TANGENT = 3,
		VERTEX_BITANGENT = 4,

		VERTEX_UV0 = 5, // TODO: Implement multipl UV channels?
		VERTEX_UV1 = 6,
		VERTEX_UV2 = 7,
		VERTEX_UV3 = 8,

		VERTEX_ATTRIBUTES_COUNT	// RESERVED: The total number of vertex attributes
	};


	struct Vertex
	{
	public:
		Vertex() :
			m_position(0.0f, 0.0f, 0.0f),
			m_tangent(1.0f, 0.0f, 0.0f),
			m_normal(0.0f, 1.0f, 0.0f),
			m_bitangent(0.0f, 0.0f, 1.0f),
			m_color(0.0f, 0.0f, 0.0f, 0.0f),
			m_uv0(0.0f, 0.0f, 0.0f, 0.0f),
			m_uv1(0.0f, 0.0f, 0.0f, 0.0f),
			m_uv2(0.0f, 0.0f, 0.0f, 0.0f),
			m_uv3(0.0f, 0.0f, 0.0f, 0.0f)
		{
		}

		 //Explicit constructor:
		Vertex(glm::vec3 const& position,
			glm::vec3 const& normal,
			glm::vec3 const tangent, 
			glm::vec3 const bitangent, 
			glm::vec4 const& color, 
			const vec4& uv0) :
			m_position(position),
			m_normal(normal),
			m_tangent(tangent),
			m_bitangent(bitangent),
			m_color(color),
			m_uv0(uv0),
			m_uv1(0.0f, 0.0f, 0.0f, 0.0f), // Just set these to 0 for now...
			m_uv2(0.0f, 0.0f, 0.0f, 0.0f),
			m_uv3(0.0f, 0.0f, 0.0f, 0.0f)
		{
		}

		// Copy constructor:
		Vertex(const Vertex& vertex) = default;

		glm::vec3 m_position;
		glm::vec4 m_color;
		
		glm::vec3 m_normal;
		glm::vec3 m_tangent;
		glm::vec3 m_bitangent;
		
		glm::vec4 m_uv0;
		glm::vec4 m_uv1;
		glm::vec4 m_uv2;
		glm::vec4 m_uv3;

	protected:


	private:


	};


	// Bounds of a mesh, scene, etc
	class Bounds
	{
	public:
		Bounds() :
			m_xMin(std::numeric_limits<float>::max()),
			m_xMax(-std::numeric_limits<float>::max()), // Note: -max is the furthest away from max
			m_yMin(std::numeric_limits<float>::max()),
			m_yMax(-std::numeric_limits<float>::max()),
			m_zMin(std::numeric_limits<float>::max()),
			m_zMax(-std::numeric_limits<float>::max())
		{
		}

		Bounds(Bounds const& rhs) = default;

		// TODO: Ensure our values give a valid 3D bounds? (ie. ?min != ?max)
		inline float& xMin() { return m_xMin; }
		inline float& xMax() { return m_xMax; }
		inline float& yMin() { return m_yMin; }
		inline float& yMax() { return m_yMax; }
		inline float& zMin() { return m_zMin; }
		inline float& zMax() { return m_zMax; }

		// Returns a Bounds, transformed from local space using transform
		Bounds GetTransformedBounds(mat4 const& m_transform);

		void Make3Dimensional();

	private:
		float m_xMin;
		float m_xMax;
		float m_yMin;
		float m_yMax;
		float m_zMin;
		float m_zMax;
	};


	class Mesh
	{
	public:
		Mesh(string name, 
			std::vector<gr::Vertex> vertices, 
			std::vector<GLuint> indices, 
			SaberEngine::Material* newMeshMaterial);
		
		/*~Mesh(); // Cleanup should be handled by whatever owns the mesh, by calling Destroy() */

		// Default copy constructor and assignment operator:
		Mesh(const Mesh& mesh)				= default;
		Mesh& operator=(Mesh const& rhs)	= default;

		// Getters/Setters:
		inline std::string& Name() { return meshName; }

		inline std::vector<Vertex>& Vertices() { return m_vertices; }
		inline size_t NumVerts() const { return m_vertices.size(); }
				
		inline std::vector<GLuint>&	Indices() { return m_indices; }
		inline size_t NumIndices() { return m_indices.size(); }

		inline SaberEngine::Material* MeshMaterial() { return m_meshMaterial; }

		inline Transform& GetTransform() { return m_transform; }

		Bounds& GetLocalBounds() { return m_localBounds; }



		// TODO: Hide API-specific implementation behind an opaque pointer:
		inline GLuint const& VAO() { return m_meshVAO; }
		inline GLuint const& VBO(VERTEX_BUFFER_OBJECT index) { return m_meshVBOs[index]; }
		
		void Bind(bool doBind);

		// Deallocate and unbind this mesh object
		void Destroy();

		
	
	protected:


	private:
		std::vector<Vertex> m_vertices;
		std::vector<GLuint> m_indices;

		SaberEngine::Material* m_meshMaterial = nullptr;

		Transform m_transform;
		std::string meshName = "UNNAMED_MESH";

		Bounds m_localBounds;	// Mesh localBounds, in local space

		// Computes mesh localBounds, in local space
		void ComputeBounds();



		// TODO: Hide API-specific implementation behind an opaque pointer:
		GLuint m_meshVAO = 0;
		GLuint m_meshVBOs[BUFFER_COUNT];			// Buffer objects that hold vertices in GPU memory
	};


	/******************************************************************************************************************/

	namespace meshfactory
	{
		inline Mesh CreateCube(SaberEngine::Material* newMeshMaterial = nullptr)
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
				glm::vec3(0.0f, 0.0f, 1.0f),		// Front = 0
				glm::vec3(0.0f, 0.0f, -1.0f),	// Back = 1
				glm::vec3(-1.0f, 0.0f, 0.0f),	// Left = 2
				glm::vec3(1.0f, 0.0f, 0.0f),		// Right = 3
				glm::vec3(0.0f, 1.0f, 0.0f),		// Up = 4
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

			std::vector<GLuint> cubeIndices // 6 faces * 2 tris * 3 indices 
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

			return Mesh("cube", cubeVerts, cubeIndices, newMeshMaterial);
		}

		inline Mesh CreateQuad(glm::vec3 tl /*= vec3(-0.5f, 0.5f, 0.0f)*/,
			glm::vec3 tr /*= vec3(0.5f, 0.5f, 0.0f)*/,
			glm::vec3 bl /*= vec3(-0.5f, -0.5f, 0.0f)*/,
			glm::vec3 br /*= vec3(0.5f, -0.5f, 0.0f)*/,
			SaberEngine::Material* newMeshMaterial = nullptr)
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

			std::vector<GLuint> quadIndices
			{
				// TL face:
				0, 1, 2,
				// BR face:
				2, 1, 3
			}; // Note: CCW winding

			return Mesh("quad", quadVerts, quadIndices, newMeshMaterial);
		}


		inline Mesh CreateSphere(float radius = 0.5f,
			size_t numLatSlices = 16,
			size_t numLongSlices = 16,
			SaberEngine::Material* newMeshMaterial = nullptr)
		{
			// NOTE: Currently, this function does not generate valid tangents for any verts. Some UV's are distorted,
			// as we're using merged vertices. TODO: Fix this

			// Note: Latitude = horizontal lines about Y
			//		Longitude = vertical lines about sphere
			//		numLatSlices = horizontal segments
			//		numLongSlices = vertical segments

			size_t numVerts = numLatSlices * numLongSlices + 2; // + 2 for end caps
			std::vector<Vertex> vertices(numVerts);
			glm::vec3* normals = new glm::vec3[numVerts];
			glm::vec4* uvs = new glm::vec4[numVerts];

			glm::vec4 vertColor(1.0f, 1.0f, 1.0f, 1.0f);

			size_t numIndices = 3 * numLatSlices * numLongSlices * 2;
			std::vector<GLuint> indices(numIndices);

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
				indices[currentIndex++] = (GLuint)0;
				indices[currentIndex++] = (GLuint)i;
				indices[currentIndex++] = (GLuint)(i + 1);
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
					indices[currentIndex++] = (GLuint)topLeft;
					indices[currentIndex++] = (GLuint)botLeft;
					indices[currentIndex++] = (GLuint)topRight;

					// Bot right triangle
					indices[currentIndex++] = (GLuint)topRight;
					indices[currentIndex++] = (GLuint)botLeft;
					indices[currentIndex++] = (GLuint)botRight;

					topLeft++;
					topRight++;
					botLeft++;
					botRight++;
				}
				// Wrap the edge around:
				// Top left triangle:
				indices[currentIndex++] = (GLuint)topLeft;
				indices[currentIndex++] = (GLuint)botLeft;
				indices[currentIndex++] = (GLuint)(topRight - numLatSlices);

				// Bot right triangle
				indices[currentIndex++] = (GLuint)(topRight - numLatSlices);
				indices[currentIndex++] = (GLuint)botLeft;
				indices[currentIndex++] = (GLuint)(botRight - numLatSlices);

				// Advance to the next row:
				topLeft++;
				botLeft++;
				topRight++;
				botRight++;
			}

			// Bottom cap:
			for (size_t i = numVerts - numLatSlices - 1; i < numVerts - 1; i++)
			{
				indices[currentIndex++] = (GLuint)i;
				indices[currentIndex++] = (GLuint)(numVerts - 1);
				indices[currentIndex++] = (GLuint)(i + 1);
			}
			indices[currentIndex - 1] = (GLuint)(numVerts - numLatSlices - 1); // Wrap the last edge back to the start		

			return Mesh("sphere", vertices, indices, newMeshMaterial);
		}
	}
}



