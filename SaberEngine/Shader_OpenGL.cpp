#include <assert.h>
#include <GL/glew.h> 

#include "DebugConfiguration.h"
#include "Shader.h"
#include "Shader_Platform.h"
#include "Shader_OpenGL.h"
#include "Material.h"
#include "Texture.h"

using std::vector;
using std::shared_ptr;
using std::string;
using std::to_string;
using gr::Texture;
using gr::Sampler;


namespace opengl
{
	void Shader::Create(gr::Shader& shader)
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
			GLchar errorMsg[1024] = { 0 }; // Error buffer

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
					glGetProgramInfoLog(shaderRef, sizeof(errorMsg), nullptr, errorMsg);
				}
				else
				{
					glGetShaderInfoLog(shaderRef, sizeof(errorMsg), nullptr, errorMsg);
				}

				SEAssert(errorMsg, false);
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
			SEAssert("No vertex shader found", shaderFiles.size() > 0);
		}

		// Create an empty shader program object:
		GLuint shaderReference = glCreateProgram();

		// Create and attach the shader stages:
		for (size_t i = 0; i < shaderFiles.size(); i++)
		{
			// Pre-process the shader text:
			platform::Shader::InsertDefines(shaderFiles[i], &shader.ShaderKeywords());
			platform::Shader::InsertIncludedFiles(shaderFiles[i]);

			// Create and attach the shader object:
			GLuint shaderObject = glCreateShader(foundShaderTypeFlags[i]);
			SEAssert("glCreateShader failed!", shaderObject > 0);

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

		// Populate the uniform locations
		// Get the number of active uniforms found in the shader:
		int numUniforms = 0;
		glGetProgramiv(params->m_shaderReference, GL_ACTIVE_UNIFORMS, &numUniforms);

		// Get the max length of the active uniform names found in the shader:
		int maxUniformNameLength = 0;
		glGetProgramiv(params->m_shaderReference, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformNameLength);

		// Store sampler uniform locations. Later, we map these locations to map samplers to texture units (ie. w/glUniform1i)
		int size = 0; // Size of the uniform variable; currently we just ignore this
		GLenum type; // Data type of the uniform
		GLchar* name = new GLchar[maxUniformNameLength]; // Uniform name, as described in the shader text
		for (size_t i = 0; i < numUniforms; i++)
		{
			glGetActiveUniform(
				params->m_shaderReference,	// program
				(GLuint)i,					// index
				maxUniformNameLength,		// buffer size
				nullptr,					// length
				&size,						// size
				&type,						// type
				&name[0]);					// name

			if (type == GL_SAMPLER_1D ||
				type == GL_SAMPLER_2D ||
				type == GL_SAMPLER_3D ||
				type == GL_SAMPLER_CUBE ||
				type == GL_SAMPLER_1D_SHADOW ||
				type == GL_SAMPLER_2D_SHADOW ||
				type == GL_SAMPLER_1D_ARRAY ||
				type == GL_SAMPLER_2D_ARRAY ||
				type == GL_SAMPLER_1D_ARRAY_SHADOW ||
				type == GL_SAMPLER_2D_ARRAY_SHADOW ||
				type == GL_SAMPLER_2D_MULTISAMPLE ||
				type == GL_SAMPLER_2D_MULTISAMPLE_ARRAY ||
				type == GL_SAMPLER_CUBE_SHADOW ||
				type == GL_SAMPLER_BUFFER ||
				type == GL_SAMPLER_2D_RECT ||
				type == GL_SAMPLER_2D_RECT_SHADOW ||
				type == GL_INT_SAMPLER_1D ||
				type == GL_INT_SAMPLER_2D ||
				type == GL_INT_SAMPLER_3D ||
				type == GL_INT_SAMPLER_CUBE ||
				type == GL_INT_SAMPLER_1D_ARRAY ||
				type == GL_INT_SAMPLER_2D_ARRAY ||
				type == GL_INT_SAMPLER_2D_MULTISAMPLE ||
				type == GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY ||
				type == GL_INT_SAMPLER_BUFFER ||
				type == GL_INT_SAMPLER_2D_RECT ||
				type == GL_UNSIGNED_INT_SAMPLER_1D ||
				type == GL_UNSIGNED_INT_SAMPLER_2D ||
				type == GL_UNSIGNED_INT_SAMPLER_3D ||
				type == GL_UNSIGNED_INT_SAMPLER_CUBE ||
				type == GL_UNSIGNED_INT_SAMPLER_1D_ARRAY ||
				type == GL_UNSIGNED_INT_SAMPLER_2D_ARRAY ||
				type == GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE ||
				type == GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY ||
				type == GL_UNSIGNED_INT_SAMPLER_BUFFER ||
				type == GL_UNSIGNED_INT_SAMPLER_2D_RECT ||
				type == GL_IMAGE_2D_MULTISAMPLE ||
				type == GL_IMAGE_2D_MULTISAMPLE_ARRAY ||
				type == GL_INT_IMAGE_2D_MULTISAMPLE ||
				type == GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY ||
				type == GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE ||
				type == GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY
				)
			{
				// Get the texture unit binding value:
				GLint val;
				glGetUniformiv(params->m_shaderReference, (GLuint)i, &val);

				// Populate the shader sampler unit map with unique entries:
				const string nameStr(name);
				SEAssert("Sampler unit already found! Does the shader have a unique binding layout qualifier?",
					params->m_samplerUnits.find(nameStr) == params->m_samplerUnits.end());

				params->m_samplerUnits.emplace(string(name), (int32_t)val);
			}			
		}
		delete[] name;

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


	void Shader::Destroy(gr::Shader& shader)
	{
		PlatformParams* const params =
			dynamic_cast<opengl::Shader::PlatformParams* const>(shader.GetPlatformParams());

		glDeleteProgram(params->m_shaderReference);
		params->m_shaderReference = 0;
	}


	void Shader::SetUniform(
		gr::Shader const& shader,
		string const& uniformName,
		void const* value, 
		platform::Shader::UniformType const type, 
		int const count)
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

		GLuint uniformID = glGetUniformLocation(params->m_shaderReference, uniformName.c_str());

		switch (type)
		{
		case platform::Shader::UniformType::Matrix4x4f:
		{
			glUniformMatrix4fv(uniformID, count, GL_FALSE, (GLfloat const*)value);
		}
		break;

		case platform::Shader::UniformType::Matrix3x3f:
		{
			glUniformMatrix3fv(uniformID, count, GL_FALSE, (GLfloat const*)value);
		}
		break;

		case platform::Shader::UniformType::Vec3f:
		{
			glUniform3fv(uniformID, count, (GLfloat const*)value);
		}
		break;

		case platform::Shader::UniformType::Vec4f:
		{
			glUniform4fv(uniformID, count, (GLfloat const*)value);
		}
		break;

		case platform::Shader::UniformType::Float:
		{
			glUniform1f(uniformID, *(GLfloat const*)value);
		}
		break;

		case platform::Shader::UniformType::Int:
		{
			glUniform1i(uniformID, *(GLint const*)value);
		}
		break;
		
		case platform::Shader::UniformType::Texture:
		{
			auto bindingUnit = params->m_samplerUnits.find(uniformName);

			SEAssert("Invalid texture name", bindingUnit != params->m_samplerUnits.end());

			static_cast<gr::Texture const*>(value)->Bind(bindingUnit->second, true);
		}
		break;
		case platform::Shader::UniformType::Sampler:
		{
			auto bindingUnit = params->m_samplerUnits.find(uniformName);

			SEAssert("Invalid sampler name", bindingUnit != params->m_samplerUnits.end());

			static_cast<gr::Sampler const*>(value)->Bind(bindingUnit->second, true);
		}
		break;
		default:
			SEAssert("Shader uniform upload failed: Recieved unimplemented uniform type", false);
		}

		// Restore the state:
		if (!isBound)
		{
			glUseProgram(currentProgram);
		}
	}
}