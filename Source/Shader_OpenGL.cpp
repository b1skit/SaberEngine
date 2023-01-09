// © 2022 Adam Badke. All rights reserved.
#include <assert.h>
#include <GL/glew.h> 

#include <array>

#include "DebugConfiguration.h"
#include "Shader.h"
#include "Shader_Platform.h"
#include "Shader_OpenGL.h"
#include "Material.h"
#include "Texture.h"
#include "Texture_OpenGL.h"
#include "ParameterBlock_OpenGL.h"
#include "PerformanceTimer.h"
#include "CoreEngine.h"

using std::vector;
using std::shared_ptr;
using std::string;
using std::to_string;
using re::Texture;
using re::Sampler;
using util::PerformanceTimer;


namespace opengl
{
	void Shader::Create(re::Shader& shader)
	{
		opengl::Shader::PlatformParams* const params =
			dynamic_cast<opengl::Shader::PlatformParams* const>(shader.GetPlatformParams());

		if (params->m_isCreated)
		{
			return;
		}
		else
		{
			params->m_isCreated = true;
		}

		string const& shaderFileName = shader.GetName();

		LOG("Creating shader: \"%s\"", shaderFileName.c_str());

		PerformanceTimer timer;
		timer.Start();

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

				SEAssertF(errorMsg);
			}
		};

		// Load the shaders, and assemble params we'll need soon:
		vector<string> shaderFiles;
		shaderFiles.reserve(numShaderTypes);
		vector<string> shaderFileNames;	// For RenderDoc markers
		shaderFiles.reserve(numShaderTypes);

		vector<uint32_t> foundShaderTypeFlags;
		foundShaderTypeFlags.reserve(numShaderTypes);

		SEAssert("Expected an entry for each shader type", shader.GetShaderTexts().size() == numShaderTypes);

		for (size_t i = 0; i < numShaderTypes; i++)
		{
			// We don't need the shader texts after loading, so we move them here
			if (!shader.GetShaderTexts()[i].empty())
			{
				shaderFiles.emplace_back(std::move(shader.GetShaderTexts()[i]));
				foundShaderTypeFlags.emplace_back(shaderTypeFlags[i]);
				shaderFileNames.emplace_back(shaderFileName + shaderFileExtensions[i]);
			}

			// We tried loading the vertex shader first, so if we hit this it means we failed to find the vertex shader
			SEAssert("No vertex shader found", shaderFiles.size() > 0);
		}
		shader.GetShaderTexts().clear(); // Remove the empty strings

		// Pre-process the shader text:
		std::atomic<uint8_t> numPreprocessed = 0;
		for (size_t i = 0; i < shaderFiles.size(); i++)
		{
			numPreprocessed++;
			en::CoreEngine::GetThreadPool()->EnqueueJob(
				[&shaderFiles, &shader, i, &numPreprocessed]() {
					platform::Shader::InsertDefines(shaderFiles[i], &shader.ShaderKeywords());
					platform::Shader::InsertIncludedFiles(shaderFiles[i]);
					numPreprocessed--;
				}
			);			
		}
		while (numPreprocessed > 0)
		{
			std::this_thread::yield();
		}

		// Create an empty shader program object:
		GLuint shaderReference = glCreateProgram();

		// Create and attach the shader stages:
		for (size_t i = 0; i < shaderFiles.size(); i++)
		{
			// Create and attach the shader object:
			GLuint shaderObject = glCreateShader(foundShaderTypeFlags[i]);
			SEAssert("glCreateShader failed!", shaderObject > 0);

			// RenderDoc object name:
			glObjectLabel(GL_SHADER, shaderObject, -1, shaderFileNames[i].c_str());

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


		LOG("Shader \"%s\" created in %f seconds", shaderFileName.c_str(), timer.StopSec());
	}


	void Shader::Bind(re::Shader& shader)
	{
		// Ensure the shader is created
		opengl::Shader::Create(shader);

		opengl::Shader::PlatformParams const* const params =
			dynamic_cast<opengl::Shader::PlatformParams const* const>(shader.GetPlatformParams());

		glUseProgram(params->m_shaderReference);
	}


	void Shader::Destroy(re::Shader& shader)
	{
		PlatformParams* const params =
			dynamic_cast<opengl::Shader::PlatformParams* const>(shader.GetPlatformParams());

		glDeleteProgram(params->m_shaderReference);
		params->m_shaderReference = 0;
		glUseProgram(0); // Unbind, as glGetIntegerv(GL_CURRENT_PROGRAM, shaderRef) still returns the shader ref otherwise
	}


	void Shader::SetUniform(
		re::Shader& shader,
		string const& uniformName,
		void* value, 
		re::Shader::UniformType const type, 
		int const count)
	{
		// Ensure the shader is created
		opengl::Shader::Create(shader);

		PlatformParams const* const params =
			dynamic_cast<opengl::Shader::PlatformParams const* const>(shader.GetPlatformParams());

		// Track if the current shader is bound or not, so we can set values without breaking the current state
		GLint currentProgram = 0;
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
		case re::Shader::UniformType::Matrix4x4f:
		{
			glUniformMatrix4fv(uniformID, count, GL_FALSE, (GLfloat const*)value);
		}
		break;

		case re::Shader::UniformType::Matrix3x3f:
		{
			glUniformMatrix3fv(uniformID, count, GL_FALSE, (GLfloat const*)value);
		}
		break;

		case re::Shader::UniformType::Vec3f:
		{
			glUniform3fv(uniformID, count, (GLfloat const*)value);
		}
		break;

		case re::Shader::UniformType::Vec4f:
		{
			glUniform4fv(uniformID, count, (GLfloat const*)value);
		}
		break;

		case re::Shader::UniformType::Float:
		{
			glUniform1f(uniformID, *(GLfloat const*)value);
		}
		break;

		case re::Shader::UniformType::Int:
		{
			glUniform1i(uniformID, *(GLint const*)value);
		}
		break;
		
		case re::Shader::UniformType::Texture:
		{
			auto const& bindingUnit = params->m_samplerUnits.find(uniformName);

#if defined(STRICT_SHADER_BINDING)
			SEAssert("Invalid texture name", bindingUnit != params->m_samplerUnits.end());
#else
			if (bindingUnit == params->m_samplerUnits.end()) return;
#endif
			opengl::Texture::Bind(*static_cast<re::Texture*>(value), bindingUnit->second);
		}
		break;
		case re::Shader::UniformType::Sampler:
		{
			auto const& bindingUnit = params->m_samplerUnits.find(uniformName);

#if defined(STRICT_SHADER_BINDING)
			SEAssert("Invalid sampler name", bindingUnit != params->m_samplerUnits.end());
#else
			if (bindingUnit == params->m_samplerUnits.end()) return;
#endif

			platform::Sampler::Bind(*static_cast<re::Sampler*>(value), bindingUnit->second);
		}
		break;
		default:
			SEAssertF("Shader uniform upload failed: Recieved unimplemented uniform type");
		}

		// Restore the state:
		if (!isBound)
		{
			glUseProgram(currentProgram);
		}
	}


	void Shader::SetParameterBlock(re::Shader& shader, re::ParameterBlock& paramBlock)
	{
		// Ensure the shader is created
		opengl::Shader::Create(shader);

		// TODO: Handle non-permanent parameter blocks. For now, just bind without considering if the data has changed

		opengl::Shader::PlatformParams const* const shaderPlatformParams =
			dynamic_cast<opengl::Shader::PlatformParams const* const>(shader.GetPlatformParams());

		// Track if the current shader is bound or not, so we can set values without breaking the current state
		GLint currentProgram = 0;
		bool isBound = true;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
		if (currentProgram != shaderPlatformParams->m_shaderReference)
		{
			glUseProgram(shaderPlatformParams->m_shaderReference);
			isBound = false;
		}
		
		// Find the buffer binding index via introspection
		const GLint resourceIdx = glGetProgramResourceIndex(
			shaderPlatformParams->m_shaderReference,	// program
			GL_SHADER_STORAGE_BLOCK,					// programInterface
			paramBlock.GetName().c_str());				// name

		SEAssert("Failed to get resource index", resourceIdx != GL_INVALID_ENUM);

#define ASSERT_ON_MISSING_RESOURCE_NAME
#if defined(ASSERT_ON_MISSING_RESOURCE_NAME)
		// GL_INVALID_INDEX is returned if name is not the name of a resource within the shader program
		SEAssert("Failed to find the resource in the shader. This is is not an error, but a useful debugging helper", 
			resourceIdx != GL_INVALID_INDEX);
#endif

		if (resourceIdx != GL_INVALID_INDEX)
		{
			GLint bindIndex;
			GLenum properties[1] = { GL_BUFFER_BINDING };
			glGetProgramResourceiv(
				shaderPlatformParams->m_shaderReference,
				GL_SHADER_STORAGE_BLOCK,
				resourceIdx,
				1,
				properties,
				1,
				NULL,
				&bindIndex);

			opengl::ParameterBlock::Bind(paramBlock, bindIndex);
		}

		// Restore the state:
		if (!isBound)
		{
			glUseProgram(currentProgram);
		}
	}


	void Shader::LoadShaderTexts(string const& extensionlessName, std::vector<std::string>& shaderTexts_out)
	{
		constexpr uint32_t numShaderTypes = 3;
		const std::array<std::string, numShaderTypes> shaderFileExtensions = {
			".vert",
			".geom",
			".frag",
		};

		shaderTexts_out.resize(numShaderTypes);
		std::atomic<uint8_t> numShadersLoaded;
		for (size_t i = 0; i < numShaderTypes; i++)
		{
			std::string assembledName = extensionlessName + shaderFileExtensions[i];
			numShadersLoaded++;
			en::CoreEngine::GetThreadPool()->EnqueueJob(
				[&shaderTexts_out, assembledName, i, &numShadersLoaded]()
				{
					shaderTexts_out[i] = std::move((platform::Shader::LoadShaderText(assembledName)));
					numShadersLoaded--;
				});
		}

		while (numShadersLoaded > 0)
		{
			std::this_thread::yield();
		}
	}
}