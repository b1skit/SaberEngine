// © 2022 Adam Badke. All rights reserved.
#include <assert.h>
#include <GL/glew.h> 

#include "Config.h"
#include "CoreEngine.h"
#include "Assert.h"
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
	constexpr uint32_t k_shaderTypeFlags[]
	{
		GL_VERTEX_SHADER,
		GL_TESS_CONTROL_SHADER,
		GL_TESS_EVALUATION_SHADER,
		GL_GEOMETRY_SHADER,
		GL_FRAGMENT_SHADER,
		GL_COMPUTE_SHADER
	};
	static_assert(_countof(k_shaderTypeFlags) == opengl::Shader::ShaderType_Count);

	constexpr char const* k_shaderFileExtensions[]
	{
		".vert",
		".tesc",
		".tese",
		".geom",
		".frag",
		".comp"
	};
	static_assert(_countof(k_shaderFileExtensions) == opengl::Shader::ShaderType_Count);


	constexpr char const* k_shaderPreambles[] // Per-shader-type preamble
	{
		// ShaderType::Vertex:
		"#define SABER_VERTEX_SHADER\n",
		
		// ShaderType::TesselationControl:
		"#define SABER_TESS_CONTROL_SHADER\n",

		// ShaderType::TesselationEvaluation:
		"#define SABER_TESS_EVALUATION_SHADER\n",

		// ShaderType::Geometry:
		"#define SABER_GEOMETRY_SHADER\n",

		// ShaderType::Fragment:
		"#define SABER_FRAGMENT_SHADER\n"
		"layout(origin_upper_left) in vec4 gl_FragCoord;\n", // Make fragment coords ([0,xRes], [0,yRes]) match our UV(0,0) = top-left convention

		// ShaderType::Compute:
		"#define SABER_COMPUTE_SHADER\n",
	};
	static_assert(_countof(k_shaderFileExtensions) == opengl::Shader::ShaderType_Count);

	constexpr char const* k_globalPreamble = "#version 460 core\n"
		"#extension GL_NV_uniform_buffer_std430_layout : require\n" // Required for UBO std430 layouts
		"\n"; // Note: MUST be terminated with "\n"


	void AssertShaderIsValid(std::string const& shaderName, uint32_t const& shaderRef, uint32_t const& flag, bool const& isProgram)
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

			SEAssertF(std::format("{}: {}", shaderName, errorMsg).c_str());
		}
	}


	string LoadShaderText(string const& filename)
	{
		// Assemble the full shader file path:
		string const& filepath = Config::Get()->GetValue<string>("shaderDirectory") + filename;

		return util::LoadTextAsString(filepath);
	}


	std::vector<std::future<void>> LoadShaderTexts(re::Shader& shader)
	{
		opengl::Shader::PlatformParams* shaderPlatformParams =
			shader.GetPlatformParams()->As<opengl::Shader::PlatformParams*>();

		std::array<std::string, opengl::Shader::ShaderType_Count>& shaderTexts = shaderPlatformParams->m_shaderTexts;

		std::vector<std::future<void>> taskFutures;
		taskFutures.resize(opengl::Shader::ShaderType_Count);
		for (size_t i = 0; i < opengl::Shader::ShaderType_Count; i++)
		{
			const std::string assembledName = shader.GetName() + k_shaderFileExtensions[i];
			taskFutures[i] = en::CoreEngine::GetThreadPool()->EnqueueJob(
				[&shaderTexts, assembledName, i]()
				{
					shaderTexts[i] = LoadShaderText(assembledName);
				});
		}

		return taskFutures;
	}


	bool InsertIncludeText(std::string const& shaderText, std::vector<std::string>& shaderTextStrings)
	{
		constexpr char const* k_includeKeyword = "#include";
		constexpr char const* k_versionKeyword = "#version";

		size_t blockStartIdx = 0;
		size_t includeStartIdx = 0;

		// Strip out any #version strings, we prepend our own. This allows us to suppress IDE warnings. 
		// The version string must be the first statement, and may not be repeated
		size_t versionIdx = shaderText.find(k_versionKeyword, 0);
		if (versionIdx != std::string::npos)
		{
			const size_t versionEndOfLineIdx = shaderText.find("\n", versionIdx + 1);
			if (versionEndOfLineIdx != std::string::npos)
			{
				blockStartIdx = versionEndOfLineIdx + 1;
				includeStartIdx = versionEndOfLineIdx + 1;
			}
		}

		
		do
		{
			includeStartIdx = shaderText.find(k_includeKeyword, blockStartIdx);
			if (includeStartIdx != string::npos)
			{
				// Check we're not on a commented line:
				size_t checkIndex = includeStartIdx;
				bool foundComment = false;
				while (checkIndex > blockStartIdx && shaderText[checkIndex] != '\n')
				{
					// -> If we hit a "#include" substring first, we've got an include
					// -> Seach until the end of the line, to strip out any trailing //comments
					if (shaderText[checkIndex] == '/' && shaderText[checkIndex - 1] == '/')
					{
						foundComment = true;
						break;
					}
					checkIndex--;
				}
				if (foundComment)
				{
					const size_t commentedIncludeEndIndex = shaderText.find("\n", includeStartIdx + 1);

					blockStartIdx = commentedIncludeEndIndex;
					continue;
				}

				size_t includeEndIndex = shaderText.find("\n", includeStartIdx + 1);
				if (includeEndIndex != string::npos)
				{
					size_t firstQuoteIndex, lastQuoteIndex;

					firstQuoteIndex = shaderText.find("\"", includeStartIdx + 1);
					if (firstQuoteIndex != string::npos && firstQuoteIndex > 0 && firstQuoteIndex < includeEndIndex)
					{
						lastQuoteIndex = shaderText.find("\"", firstQuoteIndex + 1);
						if (lastQuoteIndex != string::npos && lastQuoteIndex > firstQuoteIndex && lastQuoteIndex < includeEndIndex)
						{
							firstQuoteIndex++; // Move ahead 1 element from the first quotation mark

							const size_t includeFileNameStrLength = lastQuoteIndex - firstQuoteIndex;
							string const& includeFileName = shaderText.substr(firstQuoteIndex, includeFileNameStrLength);

							std:: string const& includeFile = LoadShaderText(includeFileName);
							if (includeFile != "")
							{
								const size_t blockLength = includeStartIdx - blockStartIdx;
								// Insert the first block
								if (blockLength > 0) // 0 if we have several consecutive #defines
								{
									shaderTextStrings.emplace_back(shaderText.substr(blockStartIdx, blockLength));
								}
								
								// Recursively parse the included file for nested #includes:
								const bool result = InsertIncludeText(includeFile, shaderTextStrings);
								if (!result)
								{
									return false;
								}
							}
							else
							{
								return false;
							}
						}
					}

					blockStartIdx = includeEndIndex + 1; // Next char that ISN'T part of the include directive substring
				}
			}
		} while (includeStartIdx != string::npos && includeStartIdx < shaderText.length());

		// Insert the last block
		if (blockStartIdx < shaderText.size())
		{
			shaderTextStrings.emplace_back(shaderText.substr(blockStartIdx, string::npos));
		}
		return true;
	}
}


namespace opengl
{
	void Shader::Create(re::Shader& shader)
	{
		PerformanceTimer timer;
		timer.Start();

		opengl::Shader::PlatformParams* params = shader.GetPlatformParams()->As<opengl::Shader::PlatformParams*>();

		SEAssert("Shader has already been created", !params->m_isCreated);
		params->m_isCreated = true;

		std::string const& shaderFileName = shader.GetName();
		LOG("Creating shader: \"%s\"", shaderFileName.c_str());
		
		// Load the individual .vert/.frag/etc shader text files:
		std::vector<std::future<void>> const& loadShaderTextsTaskFutures = LoadShaderTexts(shader);

		// Load the shaders, and assemble params we'll need soon:
		vector<string> shaderFiles;
		shaderFiles.resize(opengl::Shader::ShaderType_Count);
		vector<string> shaderFileNames;	// For RenderDoc markers
		shaderFileNames.resize(opengl::Shader::ShaderType_Count);

		// Each shader type (.vert/.frag etc) is loaded as a vector of substrings
		std::array<std::vector<std::string>, opengl::Shader::ShaderType_Count> shaderTextStrings;

		// Figure out what type of shader(s) we're loading:
		vector<uint32_t> foundShaderTypeFlags;
		foundShaderTypeFlags.resize(opengl::Shader::ShaderType_Count, 0);

		// Pre-process the shader text:
		std::vector<std::future<void>> processIncludesTaskFutures;
		processIncludesTaskFutures.resize(opengl::Shader::ShaderType_Count);
		for (size_t i = 0; i < opengl::Shader::ShaderType_Count; i++)
		{
			// Make sure we're done loading the shader texts before we continue:
			loadShaderTextsTaskFutures[i].wait();

			if (!params->m_shaderTexts[i].empty())
			{
				foundShaderTypeFlags[i] = k_shaderTypeFlags[i]; // Mark the shader as seen
				shaderFiles[i] = std::move(params->m_shaderTexts[i]); // Move the shader texts, they're no longer needed
				shaderFileNames[i] = shaderFileName + k_shaderFileExtensions[i];

				// Queue a job to parse the #include text:
				processIncludesTaskFutures[i] = en::CoreEngine::GetThreadPool()->EnqueueJob(
					[&shaderFiles, &shaderTextStrings, i]()
					{
						const bool result = InsertIncludeText(shaderFiles[i], shaderTextStrings[i]);
						SEAssert("Failed to parse shader #includes", result);
					});
			}
		}
		SEAssert("No shader found. Must have a vertex or compute shader at minimum",
			foundShaderTypeFlags[Vertex] != 0 || foundShaderTypeFlags[Compute] != 0);

		// Static so we only compute this once
		static const size_t preambleLength = strlen(k_globalPreamble);

		// Create an empty shader program object:
		params->m_shaderReference = glCreateProgram();

		// Create and attach the shader stages:
		for (size_t i = 0; i < shaderFiles.size(); i++)
		{
			if (foundShaderTypeFlags[i] == 0)
			{
				continue;
			}

			// Create and attach the shader object:
			const GLuint shaderObject = glCreateShader(foundShaderTypeFlags[i]);
			SEAssert("glCreateShader failed!", shaderObject > 0);

			// RenderDoc object name:
			glObjectLabel(GL_SHADER, shaderObject, -1, shaderFileNames[i].c_str());

			// Ensure the inclusion pre-processing task for this particular shader type is done:
			processIncludesTaskFutures[i].wait();

			// Build our list of shader string pointers for compilation:
			const size_t numShaderStrings = shaderTextStrings[i].size() + 2; // +2 for global & per-shader-type preamble

			std::vector<GLchar const*> shaderSourceStrings;
			shaderSourceStrings.resize(numShaderStrings, nullptr);
			std::vector<GLint> shaderSourceStringLengths;
			shaderSourceStringLengths.resize(numShaderStrings, 0);

			// Attach the global preamble:
			size_t insertIdx = 0;
			shaderSourceStrings[insertIdx] = k_globalPreamble;
			shaderSourceStringLengths[insertIdx] = static_cast<GLint>(preambleLength);
			insertIdx++;

			// Attach the specific shader preamble:
			shaderSourceStrings[insertIdx] = k_shaderPreambles[i];
			shaderSourceStringLengths[insertIdx] = static_cast<GLint>(strlen(k_shaderPreambles[i]));
			insertIdx++;

			// Attach the shader text substrings:			
			for (size_t shaderTextIdx = 0; shaderTextIdx < shaderTextStrings[i].size(); shaderTextIdx++)
			{
				shaderSourceStrings[insertIdx] = shaderTextStrings[i][shaderTextIdx].c_str();
				shaderSourceStringLengths[insertIdx] = static_cast<GLint>(shaderTextStrings[i][shaderTextIdx].length());
				insertIdx++;
			}

			glShaderSource(
				shaderObject, 
				static_cast<GLsizei>(shaderSourceStrings.size()),
				shaderSourceStrings.data(),
				shaderSourceStringLengths.data());
			glCompileShader(shaderObject);

			AssertShaderIsValid(shader.GetName(), shaderObject, GL_COMPILE_STATUS, false);

			glAttachShader(params->m_shaderReference, shaderObject); // Attach our shaders to the shader program

			// Delete the shader stage now that we've attached it
			glDeleteShader(shaderObject);
		}

		// Link our program object:
		glLinkProgram(params->m_shaderReference);
		AssertShaderIsValid(shader.GetName(), params->m_shaderReference, GL_LINK_STATUS, true);

		// Validate our program objects can execute with our current OpenGL state:
		glValidateProgram(params->m_shaderReference);
		AssertShaderIsValid(shader.GetName(), params->m_shaderReference, GL_VALIDATE_STATUS, true);

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
				type == GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY)
			{
				// Get the texture unit binding value:
				GLint val;
				glGetUniformiv(params->m_shaderReference, (GLuint)i, &val);

				// Populate the shader sampler unit map with unique entries:
				std::string nameStr(name);
				SEAssert("Sampler unit already found! Does the shader have a unique binding layout qualifier?",
					params->m_samplerUnits.find(nameStr) == params->m_samplerUnits.end());

				params->m_samplerUnits.emplace(std::move(nameStr), static_cast<int32_t>(val));
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


	void Shader::Bind(re::Shader const& shader)
	{
		opengl::Shader::PlatformParams const* params = 
			shader.GetPlatformParams()->As<opengl::Shader::PlatformParams const*>();

		glUseProgram(params->m_shaderReference);
	}


	void Shader::SetUniform(
		re::Shader const& shader,
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
			if (bindingUnit == params->m_samplerUnits.end())
			{
				SEAssert(std::format(
						"Shader \"{}\" texture name \"{}\"is invalid, and strict shader binding is enabled", 
						shader.GetName(), uniformName).c_str(),
					en::Config::Get()->KeyExists(en::ConfigKeys::k_strictShaderBindingCmdLineArg) == false);
				return;
			}

			opengl::Texture::Bind(*static_cast<re::Texture*>(value), bindingUnit->second);
		}
		break;
		case opengl::Shader::UniformType::Sampler:
		{
			auto const& bindingUnit = params->m_samplerUnits.find(uniformName);

			if (bindingUnit == params->m_samplerUnits.end())
			{
				SEAssert(std::format(
						"Shader \"{}\" sampler name \"{}\"is invalid, and strict shader binding is enabled", 
						shader.GetName(), uniformName).c_str(),
					en::Config::Get()->KeyExists(en::ConfigKeys::k_strictShaderBindingCmdLineArg) == false);
				return;
			}

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


	void Shader::SetParameterBlock(re::Shader const& shader, re::ParameterBlock const& paramBlock)
	{
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
		
		GLint bindIndex = 0;

		re::ParameterBlock::PlatformParams const* pbPlatformParams = paramBlock.GetPlatformParams();
		switch (pbPlatformParams->m_dataType)
		{
		case re::ParameterBlock::PBDataType::SingleElement: // Bind our single-element PBs as UBOs
		{
			// Find the buffer binding index via introspection
			const GLint uniformBlockIndex = glGetUniformBlockIndex(
				shaderPlatformParams->m_shaderReference,	// program
				paramBlock.GetName().c_str());				// Uniform block name

			// GL_INVALID_INDEX is returned if the the uniform block name does not identify an active uniform block
			SEAssert("Failed to find an active uniform block index. This is is not an error, but a useful debugging helper",
				uniformBlockIndex != GL_INVALID_INDEX ||
				en::Config::Get()->KeyExists(en::ConfigKeys::k_strictShaderBindingCmdLineArg) == false);

			if (uniformBlockIndex != GL_INVALID_INDEX)
			{
				GLenum properties[1] = { GL_BUFFER_BINDING };
				glGetProgramResourceiv(
					shaderPlatformParams->m_shaderReference,		// program
					GL_UNIFORM_BLOCK,
					uniformBlockIndex,
					1,
					properties,
					1,
					NULL,
					&bindIndex);

				// Assign binding to to an active uniform block:
				glUniformBlockBinding(
					shaderPlatformParams->m_shaderReference,	// program
					uniformBlockIndex,							// Uniform block index
					bindIndex);									// Uniform block binding
			}
		}
		break;
		case re::ParameterBlock::PBDataType::Array: // Bind our array PBs as SSBOs, as they support dynamic indexing
		{
			// Find the buffer binding index via introspection
			const GLint resourceIdx = glGetProgramResourceIndex(
			shaderPlatformParams->m_shaderReference,	// program
			GL_SHADER_STORAGE_BLOCK,					// programInterface
			paramBlock.GetName().c_str());				// name

			SEAssert("Failed to get resource index", resourceIdx != GL_INVALID_ENUM);

			// GL_INVALID_INDEX is returned if name is not the name of a resource within the shader program
			SEAssert("Failed to find the resource in the shader. This is is not an error, but a useful debugging helper",
				resourceIdx != GL_INVALID_INDEX ||
				en::Config::Get()->KeyExists(en::ConfigKeys::k_strictShaderBindingCmdLineArg) == false);

			if (resourceIdx != GL_INVALID_INDEX)
			{
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
			}
		}
		break;
		default: SEAssertF("Invalid PBDataType");
		}

		// Bind our PB to the retrieved bind index:
		opengl::ParameterBlock::Bind(paramBlock, bindIndex);

		// Restore the state:
		if (!isBound)
		{
			glUseProgram(currentProgram);
		}
	}


	void Shader::SetTextureAndSampler(
		re::Shader const& shader,
		std::string const& uniformName, 
		std::shared_ptr<re::Texture> texture,
		std::shared_ptr<re::Sampler>sampler,
		uint32_t subresource)
	{
		// Note: We don't currently use the subresource index here; OpenGL doesn't allow us to be so specific

		opengl::Shader::SetUniform(shader, uniformName, texture.get(), opengl::Shader::UniformType::Texture, 1);
		opengl::Shader::SetUniform(shader, uniformName, sampler.get(), opengl::Shader::UniformType::Sampler, 1);
	}
}