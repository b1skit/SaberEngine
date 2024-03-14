// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <GL/glew.h>

#include "Buffer_Platform.h"
#include "Buffer.h"


namespace opengl
{
	class Buffer
	{
	public:
		struct PlatformParams final : public re::Buffer::PlatformParams
		{
			GLuint m_bufferName; // UBO or SSBO handle
			GLintptr m_baseOffset; // 0 for permanent buffers, or >= 0 for single-frame buffers
		};


	public:
		static void Create(re::Buffer&);
		static void Update(re::Buffer const&, uint8_t heapOffsetFactor, uint32_t baseOffset, uint32_t numBytes);
		static void Destroy(re::Buffer&);


	public: // OpenGL-specific functionality:		
		static void Bind(re::Buffer const&, GLuint uniformBlockIdx);
	};
}