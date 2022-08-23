#include <assert.h>
#include <GL/glew.h> 

#include "BuildConfiguration.h"
#include "Shader.h"
#include "Shader_Platform.h"
#include "Shader_OpenGL.h"
#include "Material.h"
#include "Texture.h"

using std::vector;
using std::shared_ptr;
using std::string;


namespace opengl
{
	void Shader::Create(gr::Shader& shader, std::vector<std::string> const* shaderKeywords)
	{
		string const& shaderFileName = shader.Name();

		LOG("Creating shader \"" + shaderFileName + "\"");

		// Helper mappings:
		const uint32_t numShaderTypes = 3;
		const string shaderFileExtensions[numShaderTypes]
		{
			".vert",
			".geom",
			".frag",
		};
		const uint32_t shaderTypeFlags[numShaderTypes]
		{
			GL_VERTEX_SHADER,
			GL_GEOMETRY_SHADER,
			GL_FRAGMENT_SHADER
		};

		// Check shader compilation state as we progress:
		auto AssertShaderIsValid = [](uint32_t const& shaderRef, uint32_t const& flag, bool const& isProgram)
		{
			GLint success = 0;
			GLchar error[1024] = { 0 }; // Error buffer

			if (isProgram)
			{
				glGetProgramiv(shaderRef, flag, &success);
			}
			else
			{
				glGetShaderiv(shaderRef, flag, &success);
			}

			if (success == GL_FALSE)
			{
				if (isProgram)
				{
					glGetProgramInfoLog(shaderRef, sizeof(error), nullptr, error);
				}
				else
				{
					glGetShaderInfoLog(shaderRef, sizeof(error), nullptr, error);
				}

				const string errorAsString(error);

				LOG_ERROR("Shader AssertValid() failed: " + errorAsString);
				assert("Shader AssertValid() failed" && false);
			}
		};

		// Load the shaders, and assemble params we'll need soon:
		vector<string> shaderFiles;
		shaderFiles.reserve(numShaderTypes);

		vector<uint32_t> foundShaderTypeFlags;
		foundShaderTypeFlags.reserve(numShaderTypes);

		for (size_t i = 0; i < numShaderTypes; i++)
		{
			shaderFiles.emplace_back(platform::Shader::LoadShaderText(shaderFileName + shaderFileExtensions[i]));

			if (shaderFiles.back().empty())
			{
				shaderFiles.pop_back();
			}
			else
			{
				foundShaderTypeFlags.emplace_back(shaderTypeFlags[i]);
			}

			// We tried loading the vertex shader first, so if we hit this it means we failed to find the vertex shader
			if (shaderFiles.size() <= 0)
			{
				LOG_ERROR("No vertex shader found");
				assert("No vertex shader found" && false);
			}
		}

		// Create an empty shader program object:
		GLuint shaderReference = glCreateProgram();

		// Create and attach the shader stages:
		for (size_t i = 0; i < shaderFiles.size(); i++)
		{
			// Pre-process the shader text:
			platform::Shader::InsertDefines(shaderFiles[i], shaderKeywords);
			platform::Shader::InsertIncludedFiles(shaderFiles[i]);

			// Create and attach the shader object:
			GLuint shaderObject = glCreateShader(foundShaderTypeFlags[i]);

			if (shaderObject == 0)
			{
				assert("glCreateShader failed!" && false);
				LOG_ERROR("glCreateShader failed!");
			}

			vector<GLchar const*>shaderSourceStrings(1);
			vector<GLint> shaderSourceStringLengths(1);

			shaderSourceStrings[0] = shaderFiles[i].c_str();
			shaderSourceStringLengths[0] = (GLint)shaderFiles[i].length();

			glShaderSource(shaderObject, 1, &shaderSourceStrings[0], &shaderSourceStringLengths[0]);
			glCompileShader(shaderObject);

			AssertShaderIsValid(shaderObject, GL_COMPILE_STATUS, false);

			glAttachShader(shaderReference, shaderObject); // Attach our shaders to the shader program

			// Delete the shader stage now that we've attached it
			glDeleteShader(shaderObject);
		}

		// Link our program object:
		glLinkProgram(shaderReference);
		AssertShaderIsValid(shaderReference, GL_LINK_STATUS, true);

		// Validate our program objects can execute with our current OpenGL state:
		glValidateProgram(shaderReference);
		AssertShaderIsValid(shaderReference, GL_VALIDATE_STATUS, true);


		// Update our shader's platform params:
		opengl::Shader::PlatformParams* const params =
			dynamic_cast<opengl::Shader::PlatformParams* const>(shader.GetPlatformParams());
		params->m_shaderReference = shaderReference;


		// Initialize sampler locations:
		/*******************************/
		shader.Bind(true);

		// GBuffer input texture sampler locations:
		for (int slotIndex = 0; slotIndex < gr::Material::Mat_Count; slotIndex++)
		{
			GLint samplerLocation = glGetUniformLocation(
				shaderReference,
				gr::Material::k_MatTexNames[slotIndex].c_str());

			if (samplerLocation >= 0)
			{
				glUniform1i(samplerLocation, slotIndex);
			}
		}

		// Lighting GBuffer texture sampler locations:
		for (int slotIndex = 0; slotIndex < gr::Material::GBuffer_Count; slotIndex++)
		{
			GLint samplerLocation = glGetUniformLocation(
				shaderReference,
				gr::Material::k_GBufferTexNames[slotIndex].c_str());

			if (samplerLocation >= 0)
			{
				glUniform1i(samplerLocation, slotIndex);
			}
		}

		// Generic texture sampler locations:
		for (int slotIndex = 0; slotIndex < gr::Material::Tex_Count; slotIndex++)
		{
			GLint samplerLocation = glGetUniformLocation(
				shaderReference,
				gr::Material::k_GenericTexNames[slotIndex].c_str());

			if (samplerLocation >= 0)
			{
				glUniform1i(samplerLocation, slotIndex);
			}
		}

		// 2D shadow map textures sampler locations:
		for (int slotIndex = 0; slotIndex < gr::Material::Depth_Count; slotIndex++)
		{
			GLint samplerLocation = glGetUniformLocation(
				shaderReference,
				gr::Material::k_DepthTexNames[slotIndex].c_str());

			if (samplerLocation >= 0)
			{
				glUniform1i(samplerLocation, gr::Material::Depth0 + slotIndex);
			}
		}

		// Cube map depth texture sampler locations
		for (int slotIndex = 0; slotIndex < gr::Material::CubeMap_Count; slotIndex++)
		{
			GLint samplerLocation = glGetUniformLocation(
				shaderReference,
				gr::Material::k_CubeMapTexNames[slotIndex].c_str());

			if (samplerLocation >= 0)
			{
				glUniform1i(samplerLocation, (int)(gr::Material::CubeMap0 + (slotIndex * gr::Texture::k_numCubeFaces)));
			}
		}

	#if defined (DEBUG_SCENEMANAGER_SHADER_LOGGING)
		LOG("Finished creating shader \"" + shaderFileName + "\"");
	#endif
	}


	void Shader::Bind(gr::Shader const& shader, bool doBind)
	{
		opengl::Shader::PlatformParams const* const params =
			dynamic_cast<opengl::Shader::PlatformParams const* const>(shader.GetPlatformParams());

		if (doBind)
		{
			glUseProgram(params->m_shaderReference);
		}
		else
		{
			glUseProgram(0);
		}
	}


	void Shader::SetUniform(gr::Shader const& shader, char const* uniformName, void const* value, platform::Shader::UNIFORM_TYPE const& type, int count)
	{
		PlatformParams const* const params =
			dynamic_cast<opengl::Shader::PlatformParams const* const>(shader.GetPlatformParams());

		// Track if the current shader is bound or not, so we can set values without breaking the current state
		GLint currentProgram;
		bool isBound = true;	
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
		if (currentProgram != params->m_shaderReference)
		{
			glUseProgram(params->m_shaderReference);
			isBound = false;
		}

		GLuint uniformID = glGetUniformLocation(params->m_shaderReference, uniformName);
		if (uniformID >= 0)
		{
			switch (type)
			{
			case platform::Shader::UNIFORM_TYPE::Matrix4x4f:
				glUniformMatrix4fv(uniformID, count, GL_FALSE, (GLfloat const*)value);
				break;

			case platform::Shader::UNIFORM_TYPE::Matrix3x3f:
				glUniformMatrix3fv(uniformID, count, GL_FALSE, (GLfloat const*)value);
				break;

			case platform::Shader::UNIFORM_TYPE::Vec3f:
				glUniform3fv(uniformID, count, (GLfloat const*)value);
				break;

			case platform::Shader::UNIFORM_TYPE::Vec4f:
				glUniform4fv(uniformID, count, (GLfloat const*)value);
				break;

			case platform::Shader::UNIFORM_TYPE::Float:
				glUniform1f(uniformID, *(GLfloat const*)value);
				break;

			case platform::Shader::UNIFORM_TYPE::Int:
				glUniform1i(uniformID, *(GLint const*)value);
				break;

			default:
				LOG_ERROR("Shader uniform upload failed: Recieved unimplemented uniform type");
				assert("Shader uniform upload failed: Recieved unimplemented uniform type" && false);
			}
		}
		else
		{
			LOG_ERROR("Invalid uniform name received when setting shader uniform value");
			assert("Invalid uniform name received when setting shader uniform value");
		}

		// Restore the state:
		if (!isBound)
		{
			glUseProgram(currentProgram);
		}
	}


	void Shader::Destroy(gr::Shader& shader)
	{
		PlatformParams* const params =
			dynamic_cast<opengl::Shader::PlatformParams* const>(shader.GetPlatformParams());

		glDeleteProgram(params->m_shaderReference);
		params->m_shaderReference = 0;
	}
}