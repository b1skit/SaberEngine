#include "ParameterBlock_OpenGL.h"
#include "ParameterBlock.h"
#include "DebugConfiguration.h"


namespace opengl
{
	void PermanentParameterBlock::Create(re::PermanentParameterBlock& paramBlock)
	{
		LOG("Creating parameter block: \"%s\"", paramBlock.Name().c_str());
		
		PlatformParams* const params =
			dynamic_cast<opengl::PermanentParameterBlock::PlatformParams* const>(paramBlock.GetPlatformParams());

		// Generate the buffer:
		glGenBuffers(1, &params->m_ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, params->m_ssbo);
		SEAssert("Failed to generate buffer object", glIsBuffer(params->m_ssbo) == GL_TRUE);

		// RenderDoc label:
		glObjectLabel(GL_BUFFER, params->m_ssbo, -1, paramBlock.Name().c_str());

		// Buffer the data:
		glBufferData(GL_SHADER_STORAGE_BUFFER, 
			(GLsizeiptr)paramBlock.GetDataSize(), 
			paramBlock.GetData(), 
			GL_STATIC_DRAW);
	}


	void PermanentParameterBlock::Destroy(re::PermanentParameterBlock& paramBlock)
	{
		PlatformParams* const params =
			dynamic_cast<opengl::PermanentParameterBlock::PlatformParams* const>(paramBlock.GetPlatformParams());

		glDeleteBuffers(1, &params->m_ssbo);
		params->m_ssbo = 0;
	}


	void PermanentParameterBlock::Bind(re::PermanentParameterBlock const& paramBlock, GLuint bindIndex)
	{
		PlatformParams* const params =
			dynamic_cast<opengl::PermanentParameterBlock::PlatformParams* const>(paramBlock.GetPlatformParams());
		 
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindIndex, params->m_ssbo);
	}
}