// © 2022 Adam Badke. All rights reserved.
#include <assert.h>
#include <GL/glew.h> 

#include "Config.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"
#include "Material.h"
#include "ParameterBlock_OpenGL.h"
#include "PerformanceTimer.h"
#include "Sampler_OpenGL.h"
#include "Shader.h"
#include "Shader_Platform.h"
#include "Shader_OpenGL.h"
#include "TextUtils.h"
#include "Texture.h"
#include "Texture_OpenGL.h"

using en::Config;
using re::Texture;
using re::Sampler;
using util::PerformanceTimer;
using std::vector;
using std::shared_ptr;
using std::string;
using std::to_string;


namespace
{
	string LoadShaderText(string const& filename)
	{
		// Assemble the full shader file path:
		const string filepath = Config::Get()->GetValue<string>("shaderDirectory") + filename;

		return util::LoadTextAsString(filepath);
	}


	void InsertIncludedFiles(string& shaderText)
	{
		const string INCLUDE_KEYWORD = "#include";

		int foundIndex = 0;
		while (foundIndex != string::npos && foundIndex < shaderText.length())
		{
			foundIndex = (int)shaderText.find(INCLUDE_KEYWORD, foundIndex + 1);
			if (foundIndex != string::npos)
			{
				// Check we're not on a commented line:
				int checkIndex = foundIndex;
				bool foundComment = false;
				while (checkIndex >= 0 && shaderText[checkIndex] != '\n')
				{
					// TODO: Search from the beginning of the line
					// -> If we hit a "#include" substring first, we've got an include
					// -> Seach until the end of the line, to strip out any trailing //comments
					if (shaderText[checkIndex] == '/' && checkIndex > 0 && shaderText[checkIndex - 1] == '/')
					{
						foundComment = true;
						break;
					}
					checkIndex--;
				}
				if (foundComment)
				{
					continue;
				}

				int endIndex = (int)shaderText.find("\n", foundIndex + 1);
				if (endIndex != string::npos)
				{
					int firstQuoteIndex, lastQuoteIndex;

					firstQuoteIndex = (int)shaderText.find("\"", foundIndex + 1);
					if (firstQuoteIndex != string::npos && firstQuoteIndex > 0 && firstQuoteIndex < endIndex)
					{
						lastQuoteIndex = (int)shaderText.find("\"", firstQuoteIndex + 1);
						if (lastQuoteIndex != string::npos && lastQuoteIndex > firstQuoteIndex && lastQuoteIndex < endIndex)
						{
							firstQuoteIndex++; // Move ahead 1 element from the first quotation mark

							const string includeFileName = shaderText.substr(firstQuoteIndex, lastQuoteIndex - firstQuoteIndex);

							string includeFile = LoadShaderText(includeFileName);
							if (includeFile != "")
							{
								// Perform the insertion:
								string firstHalf = shaderText.substr(0, foundIndex);
								string secondHalf = shaderText.substr(endIndex + 1, shaderText.length() - 1);
								shaderText = firstHalf + includeFile + secondHalf;
							}
							else
							{
								LOG_ERROR("Could not find include file. Shader loading failed.");
								return;
							}
						}
					}
				}
			}
		}
	}


	void InsertDefines(string& shaderText, vector<string> const* shaderKeywords)
	{
		if ((int)shaderText.length() <= 0 || shaderKeywords == nullptr || (int)shaderKeywords->size() <= 0)
		{
			return;
		}

		// Find the #version directive, and insert our keywords immediately after it

		int foundIndex = (int)shaderText.find("#version", 0);
		if (foundIndex == string::npos)
		{
			foundIndex = 0;
		}
		// Find the next newline character:
		int endLine = (int)shaderText.find("\n", foundIndex + 1);

		// Assemble our #define lines:
		const string DEFINE_KEYWORD = "#define ";
		string assembledKeywords = "";
		for (int currentKeyword = 0; currentKeyword < (int)shaderKeywords->size(); currentKeyword++)
		{
			string defineLine = DEFINE_KEYWORD + shaderKeywords->at(currentKeyword) + "\n";

			assembledKeywords += defineLine;
		}

		// Insert our #define lines:
		shaderText.insert(endLine + 1, assembledKeywords);
	}
}


namespace opengl
{
	void Shader::Create(re::Shader& shader)
	{
		opengl::Shader::PlatformParams* params = shader.GetPlatformParams()->As<opengl::Shader::PlatformParams*>();

		SEAssert("Shader has already been created", !params->m_isCreated);
		params->m_isCreated = true;
		
		opengl::Shader::LoadShaderTexts(shader);

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

		SEAssert("Expected an entry for each shader type", params->m_shaderTexts.size() == numShaderTypes);

		for (size_t i = 0; i < numShaderTypes; i++)
		{
			// We don't need the shader texts after loading, so we move them here
			if (!params->m_shaderTexts[i].empty())
			{
				shaderFiles.emplace_back(std::move(params->m_shaderTexts[i]));
				foundShaderTypeFlags.emplace_back(shaderTypeFlags[i]);
				shaderFileNames.emplace_back(shaderFileName + shaderFileExtensions[i]);
			}

			// We tried loading the vertex shader first, so if we hit this it means we failed to find the vertex shader
			SEAssert("No vertex shader found", shaderFiles.size() > 0);
		}
		params->m_shaderTexts.clear(); // Remove the empty strings

		// Pre-process the shader text:
		std::atomic<uint8_t> numPreprocessed = 0;
		for (size_t i = 0; i < shaderFiles.size(); i++)
		{
			numPreprocessed++;
			en::CoreEngine::GetThreadPool()->EnqueueJob(
				[&shaderFiles, &shader, i, &numPreprocessed]() {
					InsertDefines(shaderFiles[i], &shader.ShaderKeywords());
					InsertIncludedFiles(shaderFiles[i]);
					numPreprocessed--;
				}
			);			
		}

		// TODO: Replace this with a condition variable
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


	void Shader::Destroy(re::Shader& shader)
	{
		PlatformParams* params = shader.GetPlatformParams()->As<opengl::Shader::PlatformParams*>();
		if (!params->m_isCreated)
		{
			return;
		}
		params->m_isCreated = false;

		glDeleteProgram(params->m_shaderReference);
		params->m_shaderReference = 0;
		glUseProgram(0); // Unbind, as glGetIntegerv(GL_CURRENT_PROGRAM, shaderRef) still returns the shader ref otherwise
	}


	void Shader::Bind(re::Shader& shader)
	{
		opengl::Shader::PlatformParams const* params = 
			shader.GetPlatformParams()->As<opengl::Shader::PlatformParams const*>();

		glUseProgram(params->m_shaderReference);
	}


	void Shader::SetUniform(
		re::Shader& shader,
		string const& uniformName,
		void* value, 
		opengl::Shader::UniformType const type, 
		int const count)
	{
		PlatformParams const* params = shader.GetPlatformParams()->As<opengl::Shader::PlatformParams const*>();
		SEAssert("Shader has not been created yet", params->m_isCreated == true);

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
		case opengl::Shader::UniformType::Matrix4x4f:
		{
			glUniformMatrix4fv(uniformID, count, GL_FALSE, (GLfloat const*)value);
		}
		break;

		case opengl::Shader::UniformType::Matrix3x3f:
		{
			glUniformMatrix3fv(uniformID, count, GL_FALSE, (GLfloat const*)value);
		}
		break;

		case opengl::Shader::UniformType::Vec3f:
		{
			glUniform3fv(uniformID, count, (GLfloat const*)value);
		}
		break;

		case opengl::Shader::UniformType::Vec4f:
		{
			glUniform4fv(uniformID, count, (GLfloat const*)value);
		}
		break;

		case opengl::Shader::UniformType::Float:
		{
			glUniform1f(uniformID, *(GLfloat const*)value);
		}
		break;

		case opengl::Shader::UniformType::Int:
		{
			glUniform1i(uniformID, *(GLint const*)value);
		}
		break;
		
		case opengl::Shader::UniformType::Texture:
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
		case opengl::Shader::UniformType::Sampler:
		{
			auto const& bindingUnit = params->m_samplerUnits.find(uniformName);

#if defined(STRICT_SHADER_BINDING)
			SEAssert("Invalid sampler name", bindingUnit != params->m_samplerUnits.end());
#else
			if (bindingUnit == params->m_samplerUnits.end()) return;
#endif

			opengl::Sampler::Bind(*static_cast<re::Sampler*>(value), bindingUnit->second);
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
		// TODO: Handle non-permanent parameter blocks. For now, just bind without considering if the data has changed

		opengl::Shader::PlatformParams const* shaderPlatformParams = 
			shader.GetPlatformParams()->As<opengl::Shader::PlatformParams const*>();

		SEAssert("Shader has not been created yet", shaderPlatformParams->m_isCreated == true);

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


	void Shader::SetTextureAndSampler(
		re::Shader& shader, std::string const& uniformName, std::shared_ptr<re::Texture> texture, std::shared_ptr<re::Sampler>sampler)
	{
		opengl::Shader::SetUniform(shader, uniformName, texture.get(), opengl::Shader::UniformType::Texture, 1);
		opengl::Shader::SetUniform(shader, uniformName, sampler.get(), opengl::Shader::UniformType::Sampler, 1);
	}


	void Shader::LoadShaderTexts(re::Shader& shader)
	{
		opengl::Shader::PlatformParams* shaderPlatformParams = 
			shader.GetPlatformParams()->As<opengl::Shader::PlatformParams*>();

		std::vector<std::string>& shaderTexts = shaderPlatformParams->m_shaderTexts;
		shaderTexts.clear();

		constexpr uint32_t numShaderTypes = 3;
		const std::array<std::string, numShaderTypes> shaderFileExtensions = {
			".vert",
			".geom",
			".frag",
		};

		shaderTexts.resize(numShaderTypes);
		std::atomic<uint8_t> numShadersLoaded;
		for (size_t i = 0; i < numShaderTypes; i++)
		{
			std::string assembledName = shader.GetName() + shaderFileExtensions[i];
			numShadersLoaded++;
			en::CoreEngine::GetThreadPool()->EnqueueJob(
				[&shaderTexts, assembledName, i, &numShadersLoaded]()
				{
					shaderTexts[i] = std::move((LoadShaderText(assembledName)));
					numShadersLoaded--;
				});
		}

		// TODO: Use a condition variable
		while (numShadersLoaded > 0)
		{
			std::this_thread::yield();
		}
	}
}