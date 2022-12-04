#include "ParameterBlock_OpenGL.h"
#include "ParameterBlock.h"
#include "DebugConfiguration.h"


namespace opengl
{
	void ParameterBlock::Create(re::ParameterBlock& paramBlock)
	{
		PlatformParams* const params =
			dynamic_cast<opengl::ParameterBlock::PlatformParams* const>(paramBlock.GetPlatformParams());

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

		void* data;
		size_t numBytes;
		paramBlock.GetDataAndSize(data, numBytes);

		// Buffer the data:
		glBufferData(GL_SHADER_STORAGE_BUFFER, 
			(GLsizeiptr)numBytes,
			data,
			GL_STATIC_DRAW);

		paramBlock.MarkClean();
	}


	void ParameterBlock::Update(re::ParameterBlock& paramBlock)
	{
		PlatformParams* const params =
			dynamic_cast<opengl::ParameterBlock::PlatformParams* const>(paramBlock.GetPlatformParams());

		// Ensure the PB is created before we attempt to update it
		opengl::ParameterBlock::Create(paramBlock);

		void* data;
		size_t numBytes;
		paramBlock.GetDataAndSize(data, numBytes);

		glNamedBufferSubData(params->m_ssbo, 0, (GLsizeiptr)numBytes, data);

		paramBlock.MarkClean();
	}


	void ParameterBlock::Destroy(re::ParameterBlock& paramBlock)
	{
		PlatformParams* const params =
			dynamic_cast<opengl::ParameterBlock::PlatformParams* const>(paramBlock.GetPlatformParams());

		glDeleteBuffers(1, &params->m_ssbo);
		params->m_ssbo = 0;
	}


	void ParameterBlock::Bind(re::ParameterBlock& paramBlock, GLuint bindIndex)
	{
		// Ensure the PB is created before we attempt to bind it
		opengl::ParameterBlock::Create(paramBlock);

		PlatformParams* const params =
			dynamic_cast<opengl::ParameterBlock::PlatformParams* const>(paramBlock.GetPlatformParams());
		 
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindIndex, params->m_ssbo);
	}
}