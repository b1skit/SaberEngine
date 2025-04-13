// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Buffer.h"

#include <GL/glew.h>


namespace re
{
	class BufferView;
}

namespace opengl
{
	class Buffer
	{
	public:
		struct PlatObj final : public re::Buffer::PlatObj
		{
			void Destroy() override;

			GLuint m_bufferName = 0;	// UBO or SSBO handle
			GLintptr m_baseByteOffset = 0;	// 0 for permanent buffers, or >= 0 for single-frame buffers

			bool m_isSharedBufferName = false; // If true, do not call glDeleteBuffers() in Destroy()
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

		static void const* MapCPUReadback(re::Buffer const&, uint8_t frameLatency);
		static void UnmapCPUReadback(re::Buffer const&);


	public: // OpenGL-specific functionality:		
		static void Bind(re::Buffer const&, BindTarget, re::BufferView const&, GLuint bindIndex);
	};
}