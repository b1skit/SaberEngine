// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Buffer_Platform.h"
#include "Buffer.h"

#include <GL/glew.h>


namespace opengl
{
	class Buffer
	{
	public:
		struct PlatformParams final : public re::Buffer::PlatformParams
		{
			GLuint m_bufferName = 0; // UBO or SSBO handle
			GLintptr m_baseOffset = 0; // 0 for permanent buffers, or >= 0 for single-frame buffers
		};

		enum BindTarget
		{
			UBO,
			SSBO,
			Vertex,
			Index,

			BindTarget_Count
		};

	public:
		static void Create(re::Buffer&);
		static void Update(re::Buffer const&, uint8_t heapOffsetFactor, uint32_t baseOffset, uint32_t numBytes);
		static void Destroy(re::Buffer&);

		static void const* MapCPUReadback(re::Buffer const&, uint8_t frameLatency);
		static void UnmapCPUReadback(re::Buffer const&);


	public: // OpenGL-specific functionality:		
		static void Bind(re::Buffer const&, BindTarget, GLuint bindIndex);
	};
}