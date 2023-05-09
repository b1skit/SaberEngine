// © 2022 Adam Badke. All rights reserved.
#include "ParameterBlock_OpenGL.h"
#include "ParameterBlock.h"
#include "DebugConfiguration.h"


namespace opengl
{
	void ParameterBlock::Create(re::ParameterBlock& paramBlock)
	{
		PlatformParams* params = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();

		if (params->m_isCreated)
		{
			return;
		}
		params->m_isCreated = true;

		// Generate the buffer:
		glGenBuffers(1, &params->m_ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, params->m_ssbo);
		SEAssert("Failed to generate buffer object", glIsBuffer(params->m_ssbo) == GL_TRUE);

		// RenderDoc label:
		glObjectLabel(GL_BUFFER, params->m_ssbo, -1, paramBlock.GetName().c_str());

		void const* data;
		size_t numBytes;
		paramBlock.GetDataAndSize(data, numBytes);

		// Buffer the data:
		glBufferData(GL_SHADER_STORAGE_BUFFER, 
			(GLsizeiptr)numBytes,
			data,
			GL_STATIC_DRAW);
	}


	void ParameterBlock::Update(re::ParameterBlock& paramBlock)
	{
		// Ensure the PB is created before we attempt to update it
		opengl::ParameterBlock::Create(paramBlock);

		PlatformParams* params = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();

		void const* data;
		size_t numBytes;
		paramBlock.GetDataAndSize(data, numBytes);

		glNamedBufferSubData(
			params->m_ssbo,			// Target
			0,						// Offset
			(GLsizeiptr)numBytes,	// Size
			data);					// Data
	}


	void ParameterBlock::Destroy(re::ParameterBlock& paramBlock)
	{
		PlatformParams* params = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();
		if (!params->m_isCreated)
		{
			return;
		}

		glDeleteBuffers(1, &params->m_ssbo);
		params->m_ssbo = 0;

		params->m_isCreated = false;
	}


	void ParameterBlock::Bind(re::ParameterBlock& paramBlock, GLuint bindIndex)
	{
		// Ensure the PB is created before we attempt to bind it
		opengl::ParameterBlock::Create(paramBlock);

		PlatformParams* params = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();
		 
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindIndex, params->m_ssbo);
	}
}