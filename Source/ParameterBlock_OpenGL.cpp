// © 2022 Adam Badke. All rights reserved.
#include "ParameterBlock_OpenGL.h"
#include "ParameterBlock.h"
#include "DebugConfiguration.h"


namespace opengl
{
	void ParameterBlock::Create(re::ParameterBlock& paramBlock)
	{
		PlatformParams* params = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();
		SEAssert("Parameter block is already created", !params->m_isCreated);
		params->m_isCreated = true;

		// Generate the buffer name:
		glGenBuffers(1, &params->m_ssbo);

		// Binding associates the buffer object with the buffer object name
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, params->m_ssbo);
		SEAssert("Failed to generate buffer object", glIsBuffer(params->m_ssbo) == GL_TRUE);

		// RenderDoc label:
		glObjectLabel(GL_BUFFER, params->m_ssbo, -1, paramBlock.GetName().c_str());

		void const* data;
		size_t numBytes;
		paramBlock.GetDataAndSize(data, numBytes);

		glBufferData(GL_SHADER_STORAGE_BUFFER,
			(GLsizeiptr)numBytes,
			nullptr, // NULL: Data store of the specified size is created, but remains uninitialized and thus undefined
			GL_STATIC_DRAW);
	}


	void ParameterBlock::Update(re::ParameterBlock& paramBlock)
	{
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
		SEAssert("Attempting to destroy a ParameterBlock that has not been created", params->m_isCreated);

		glDeleteBuffers(1, &params->m_ssbo);
		params->m_ssbo = 0;
		params->m_isCreated = false;
	}


	void ParameterBlock::Bind(re::ParameterBlock& paramBlock, GLuint bindIndex)
	{
		PlatformParams* params = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();
		 
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindIndex, params->m_ssbo);
	}
}