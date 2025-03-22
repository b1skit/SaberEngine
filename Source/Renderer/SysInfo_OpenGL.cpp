// © 2023 Adam Badke. All rights reserved.
#include "Core/Assert.h"
#include "Core/Util/CastUtils.h"
#include "SysInfo_OpenGL.h"


// Note: Most of these functions can/will likely be called from the main thread. Follow the pattern of caching the
// result in a static variable and priming it from the main thread during startup by calling from the opengl::Context
namespace opengl
{
	uint8_t SysInfo::GetMaxRenderTargets()
	{
		// NOTE: This can only be called from the main thread, so we cache the result in a static variable and call this
		// during opengl::Context initialization
		static GLint s_maxColorAttachments = 0;
		if (s_maxColorAttachments == 0)
		{
			glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &s_maxColorAttachments);
		}
		return s_maxColorAttachments;
	}


	uint8_t SysInfo::GetMaxVertexAttributes()
	{
		static GLint s_maxVertexAttributes = 0;
		if (s_maxVertexAttributes == 0)
		{
			glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &s_maxVertexAttributes);
		}
		return s_maxVertexAttributes;
	}


	bool SysInfo::BindlessResourcesSupported()
	{
		return false;
	}


	GLint SysInfo::GetUniformBufferOffsetAlignment()
	{
		static GLint s_uniformBufferOffsetAlignment = 0;
		if (s_uniformBufferOffsetAlignment == 0)
		{
			glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &s_uniformBufferOffsetAlignment);
		}
		return s_uniformBufferOffsetAlignment;
	}


	GLint SysInfo::GetShaderStorageBufferOffsetAlignment()
	{
		static GLint s_shaderStorageBufferOffsetAlignment = 0;
		if (s_shaderStorageBufferOffsetAlignment == 0)
		{
			glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &s_shaderStorageBufferOffsetAlignment);
		}
		return s_shaderStorageBufferOffsetAlignment;
	}


	uint32_t SysInfo::GetMaxUniformBufferBindings(re::Shader::ShaderType shaderType)
	{
		switch (shaderType)
		{
		case re::Shader::ShaderType::Vertex:
		{
			static GLint s_maxVertexUniformBufferBindings = 0;
			if (s_maxVertexUniformBufferBindings == 0)
			{
				glGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS, &s_maxVertexUniformBufferBindings);
			}
			return s_maxVertexUniformBufferBindings;
		}
		break;
		case re::Shader::ShaderType::Geometry:
		{
			static GLint s_maxGeometryUniformBufferBindings = 0;
			if (s_maxGeometryUniformBufferBindings == 0)
			{
				glGetIntegerv(GL_MAX_GEOMETRY_UNIFORM_BLOCKS, &s_maxGeometryUniformBufferBindings);
			}
			return s_maxGeometryUniformBufferBindings;
		}
		break;
		case re::Shader::ShaderType::Pixel:
		{
			static GLint s_maxFragmentUniformBufferBindings = 0;
			if (s_maxFragmentUniformBufferBindings == 0)
			{
				glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &s_maxFragmentUniformBufferBindings);
			}
			return s_maxFragmentUniformBufferBindings;
		}
		break;
		case re::Shader::ShaderType::Hull:
		{
			static GLint s_maxTessCtrlUniformBufferBindings = 0;
			if (s_maxTessCtrlUniformBufferBindings == 0)
			{
				glGetIntegerv(GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS, &s_maxTessCtrlUniformBufferBindings);
			}
			return s_maxTessCtrlUniformBufferBindings;
		}
		break;
		case re::Shader::ShaderType::Domain:
		{
			static GLint s_maxTessEvalUniformBufferBindings = 0;
			if (s_maxTessEvalUniformBufferBindings == 0)
			{
				glGetIntegerv(GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS, &s_maxTessEvalUniformBufferBindings);
			}
			return s_maxTessEvalUniformBufferBindings;
		}
		break;
		case re::Shader::ShaderType::Mesh:
		{
			SEAssertF("Mesh shaders are not (currently) supported on OpenGL");
		}
		break;
		case re::Shader::ShaderType::Amplification:
		{
			SEAssertF("Amplification shaders are not (currently) supported on OpenGL");
		}
		break;
		case re::Shader::ShaderType::Compute:
		{
			static GLint s_maxComputeUniformBufferBindings = 0;
			if (s_maxComputeUniformBufferBindings == 0)
			{
				glGetIntegerv(GL_MAX_COMPUTE_UNIFORM_BLOCKS, &s_maxComputeUniformBufferBindings);
			}
			return s_maxComputeUniformBufferBindings;
		}
		break;
		default: SEAssertF("Invalid shader type");
		}
		return 0; // This should never happen
	}


	uint32_t SysInfo::GetMaxShaderStorageBlockBindings(re::Shader::ShaderType shaderType)
	{
		switch (shaderType)
		{
		case re::Shader::ShaderType::Vertex:
		{
			static GLint s_maxVertexShaderStorageBufferBindings = 0;
			if (s_maxVertexShaderStorageBufferBindings == 0)
			{
				glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &s_maxVertexShaderStorageBufferBindings);
			}
			return s_maxVertexShaderStorageBufferBindings;
		}
		break;
		case re::Shader::ShaderType::Geometry:
		{
			static GLint s_maxGeometryShaderStorageBufferBindings = 0;
			if (s_maxGeometryShaderStorageBufferBindings == 0)
			{
				glGetIntegerv(GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS, &s_maxGeometryShaderStorageBufferBindings);
			}
			return s_maxGeometryShaderStorageBufferBindings;
		}
		break;
		case re::Shader::ShaderType::Pixel:
		{
			static GLint s_maxFragmentShaderStorageBufferBindings = 0;
			if (s_maxFragmentShaderStorageBufferBindings == 0)
			{
				glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &s_maxFragmentShaderStorageBufferBindings);
			}
			return s_maxFragmentShaderStorageBufferBindings;
		}
		break;
		case re::Shader::ShaderType::Hull:
		{
			static GLint s_maxTessCtrlShaderStorageBufferBindings = 0;
			if (s_maxTessCtrlShaderStorageBufferBindings == 0)
			{
				glGetIntegerv(GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS, &s_maxTessCtrlShaderStorageBufferBindings);
			}
			return s_maxTessCtrlShaderStorageBufferBindings;
		}
		break;
		case re::Shader::ShaderType::Domain:
		{
			static GLint s_maxTessEvalShaderStorageBufferBindings = 0;
			if (s_maxTessEvalShaderStorageBufferBindings == 0)
			{
				glGetIntegerv(GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS, &s_maxTessEvalShaderStorageBufferBindings);
			}
			return s_maxTessEvalShaderStorageBufferBindings;
		}
		break;
		case re::Shader::ShaderType::Mesh:
		{
			SEAssertF("Mesh shaders are not (currently) supported on OpenGL");
		}
		break;
		case re::Shader::ShaderType::Amplification:
		{
			SEAssertF("Amplification shaders are not (currently) supported on OpenGL");
		}
		break;
		case re::Shader::ShaderType::Compute:
		{
			static GLint s_maxComputeShaderStorageBufferBindings = 0;
			if (s_maxComputeShaderStorageBufferBindings == 0)
			{
				glGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, &s_maxComputeShaderStorageBufferBindings);
			}
			return s_maxComputeShaderStorageBufferBindings;
		}
		break;
		default: SEAssertF("Invalid shader type");
		}
		return 0; // This should never happen
	}


	uint8_t SysInfo::GetMaxTextureBindPoints()
	{
		static uint8_t s_maxCombinedTexInputs = 0;
		if (s_maxCombinedTexInputs == 0)
		{
			GLint maxTexInputs = 0;
			glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTexInputs);
			s_maxCombinedTexInputs = util::CheckedCast<uint8_t>(maxTexInputs);
		}
		return s_maxCombinedTexInputs;
	}


	GLint SysInfo::GetMaxAnisotropy()
	{
		static GLint s_maxAnisotropy = 0;
		if (s_maxAnisotropy == 0)
		{
			glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &s_maxAnisotropy);
		}
		return s_maxAnisotropy;
	}
}