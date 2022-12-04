#pragma once

#include <GL/glew.h>

#include "MeshPrimitive_Platform.h"
#include "MeshPrimitive.h"


namespace gr
{
	class MeshPrimitive;
}

namespace opengl
{
	class MeshPrimitive
	{
	public:
		enum VertexAttribute
		{
			Position	= 0,
			Normal		= 1,
			Tangent		= 2,
			UV0			= 3,
			Color		= 4,

			Indexes, // Not assigned a layout binding location

			VertexAttribute_Count
		};
		// Note: The order/indexing of this enum MUST match the vertex layout locations in SaberCommon.glsl


		struct PlatformParams final : public virtual re::MeshPrimitive::PlatformParams
		{
			PlatformParams(re::MeshPrimitive& meshPrimitive);
			~PlatformParams() override = default;

			GLuint m_meshVAO; // Vertex array object

			// IDs for buffer objects that hold vertex stream data in GPU memory (equivalent to D3D vertex buffers)
			std::vector<GLuint> m_meshVBOs;

			GLenum m_drawMode = GL_TRIANGLES;
		};

		static void Create(re::MeshPrimitive& meshPrimitive);
		static void Bind(re::MeshPrimitive& meshPrimitive, bool doBind);
		static void Destroy(re::MeshPrimitive& meshPrimitive);
	};
}