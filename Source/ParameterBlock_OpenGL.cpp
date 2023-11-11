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
		glGenBuffers(1, &params->m_bufferName);

		GLenum bufferTarget = 0;
		switch (params->m_dataType)
		{
		case re::ParameterBlock::PBDataType::SingleElement:
		{
			bufferTarget = GL_UNIFORM_BUFFER;
		}
		break;
		case re::ParameterBlock::PBDataType::Array:
		{
			bufferTarget = GL_SHADER_STORAGE_BUFFER;
		}
		break;
		default: SEAssertF("Invalid PBDataType");
		}

		// Binding associates the buffer object with the buffer object name
		glBindBuffer(bufferTarget, params->m_bufferName);
		SEAssert("Failed to generate buffer object", glIsBuffer(params->m_bufferName) == GL_TRUE);

		void const* data;
		uint32_t numBytes;
		paramBlock.GetDataAndSize(data, numBytes);

		glBufferData(
			bufferTarget,
			(GLsizeiptr)numBytes,
			nullptr, // NULL: Data store of the specified size is created, but remains uninitialized and thus undefined
			paramBlock.GetType() == re::ParameterBlock::PBType::Immutable ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);

		// RenderDoc label:
		glObjectLabel(GL_BUFFER, params->m_bufferName, -1, paramBlock.GetName().c_str());
	}


	void ParameterBlock::Update(re::ParameterBlock& paramBlock)
	{
		PlatformParams* params = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();

		void const* data;
		uint32_t numBytes;
		paramBlock.GetDataAndSize(data, numBytes);

		glNamedBufferSubData(
			params->m_bufferName,	// Target
			0,						// Offset
			(GLsizeiptr)numBytes,	// Size
			data);					// Data
	}


	void ParameterBlock::Destroy(re::ParameterBlock& paramBlock)
	{
		PlatformParams* params = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();
		SEAssert("Attempting to destroy a ParameterBlock that has not been created", params->m_isCreated);

		glDeleteBuffers(1, &params->m_bufferName);
		params->m_bufferName = 0;
		params->m_isCreated = false;
	}


	void ParameterBlock::Bind(re::ParameterBlock const& paramBlock, GLuint bindIndex)
	{
		PlatformParams* params = paramBlock.GetPlatformParams()->As<opengl::ParameterBlock::PlatformParams*>();
		 
		GLenum bufferTarget = 0;
		switch (params->m_dataType)
		{
		case re::ParameterBlock::PBDataType::SingleElement:
		{
			bufferTarget = GL_UNIFORM_BUFFER;
		}
		break;
		case re::ParameterBlock::PBDataType::Array:
		{
			bufferTarget = GL_SHADER_STORAGE_BUFFER;
		}
		break;
		default: SEAssertF("Invalid PBDataType");
		}

		glBindBufferBase(bufferTarget, bindIndex, params->m_bufferName);
	}
}