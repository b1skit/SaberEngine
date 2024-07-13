// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
#include "Buffer_OpenGL.h"
#include "Material.h"
#include "Sampler_OpenGL.h"
#include "Shader.h"
#include "Shader_Platform.h"
#include "Shader_OpenGL.h"
#include "Texture.h"
#include "Texture_OpenGL.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/PerformanceTimer.h"
#include "Core/ThreadPool.h"
#include "Core/Util/TextUtils.h"

#include <GL/glew.h> 


namespace
{
	constexpr uint32_t k_shaderTypeFlags[]
	{
		GL_VERTEX_SHADER,
		GL_GEOMETRY_SHADER,
		GL_FRAGMENT_SHADER,

		GL_TESS_CONTROL_SHADER,
		GL_TESS_EVALUATION_SHADER,

		GL_MESH_SHADER_BIT_NV,
		GL_TASK_SHADER_BIT_NV,

		GL_COMPUTE_SHADER
	};
	static_assert(_countof(k_shaderTypeFlags) == re::Shader::ShaderType_Count);

	constexpr char const* k_shaderFileExtensions[]
	{
		".vert",
		".geom",
		".frag",

		".tesc",
		".tese",

		".mesh",
		".task",

		".comp"
	};
	static_assert(_countof(k_shaderFileExtensions) == re::Shader::ShaderType_Count);


	constexpr char const* k_shaderPreambles[] // Per-shader-type preamble
	{
		// ShaderType::Vertex:
		"#define SE_VERTEX_SHADER\n",
		
		// ShaderType::Geometry:
		"#define SE_GEOMETRY_SHADER\n",

		// ShaderType::Fragment:
		"#define SE_FRAGMENT_SHADER\n"
		"layout(origin_upper_left) in vec4 gl_FragCoord;\n", // Make fragment coords ([0,xRes), [0,yRes)) match our UV(0,0) = top-left convention


		// ShaderType::TesselationControl:
		"#define SE_TESS_CONTROL_SHADER\n",

		// ShaderType::TesselationEvaluation:
		"#define SE_TESS_EVALUATION_SHADER\n",


		// ShaderType::Mesh:
		"#define SE_MESH_SHADER\n",

		// ShaderType::Amplification:
		"#define SE_TASK_SHADER\n",


		// ShaderType::Compute:
		"#define SE_COMPUTE_SHADER\n",
	};
	static_assert(_countof(k_shaderPreambles) == re::Shader::ShaderType_Count);


	constexpr char const* k_globalPreamble = "#version 460 core\n"
		"#extension GL_NV_uniform_buffer_std430_layout : require\n" // Required for UBO std430 layouts
		"#define SE_OPENGL\n"
		"\n"; // Note: MUST be terminated with "\n"


	bool UniformIsSamplerType(GLenum type)
	{
		return 
			// GL_VERSION_2_0:
			type == GL_SAMPLER_1D ||
			type == GL_SAMPLER_2D ||
			type == GL_SAMPLER_3D ||
			type == GL_SAMPLER_CUBE ||
			type == GL_SAMPLER_1D_SHADOW ||
			type == GL_SAMPLER_2D_SHADOW ||
			// GL_VERSION_3_0:
			type == GL_SAMPLER_1D_ARRAY ||
			type == GL_SAMPLER_2D_ARRAY ||
			type == GL_SAMPLER_1D_ARRAY_SHADOW ||
			type == GL_SAMPLER_2D_ARRAY_SHADOW ||
			type == GL_SAMPLER_CUBE_SHADOW ||
			type == GL_INT_SAMPLER_1D ||
			type == GL_INT_SAMPLER_2D ||
			type == GL_INT_SAMPLER_3D ||
			type == GL_INT_SAMPLER_CUBE ||
			type == GL_INT_SAMPLER_1D_ARRAY ||
			type == GL_INT_SAMPLER_2D_ARRAY ||
			type == GL_UNSIGNED_INT_SAMPLER_1D ||
			type == GL_UNSIGNED_INT_SAMPLER_2D ||
			type == GL_UNSIGNED_INT_SAMPLER_3D ||
			type == GL_UNSIGNED_INT_SAMPLER_CUBE ||
			type == GL_UNSIGNED_INT_SAMPLER_1D_ARRAY ||
			type == GL_UNSIGNED_INT_SAMPLER_2D_ARRAY ||
			// GL_VERSION_3_1:
			type == GL_SAMPLER_2D_RECT ||
			type == GL_SAMPLER_2D_RECT_SHADOW ||
			type == GL_SAMPLER_BUFFER ||
			type == GL_INT_SAMPLER_2D_RECT ||
			type == GL_INT_SAMPLER_BUFFER ||
			type == GL_UNSIGNED_INT_SAMPLER_2D_RECT ||
			type == GL_UNSIGNED_INT_SAMPLER_BUFFER ||
			// GL_VERSION_4_0:
			type == GL_SAMPLER_CUBE_MAP_ARRAY ||
			type == GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW ||
			type == GL_INT_SAMPLER_CUBE_MAP_ARRAY ||
			type == GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY ||
			// GL_ARB_texture_multisample
			type == GL_SAMPLER_2D_MULTISAMPLE ||
			type == GL_INT_SAMPLER_2D_MULTISAMPLE ||
			type == GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE ||
			type == GL_SAMPLER_2D_MULTISAMPLE_ARRAY ||
			type == GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY ||
			type == GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY ||
			// GL_ARB_shader_image_load_store (NOTE: Not all types are included here (yet?))
			type == GL_IMAGE_2D_MULTISAMPLE ||
			type == GL_IMAGE_2D_MULTISAMPLE_ARRAY ||
			type == GL_INT_IMAGE_2D_MULTISAMPLE ||
			type == GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY ||
			type == GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE ||
			type == GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY;
	}


	void AssertShaderIsValid(
		std::string const& shaderName, uint32_t const& shaderRef, uint32_t const& flag, bool const& isProgram)
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


	std::string LoadShaderText(std::string const& filenameAndExtension)
	{
		// Assemble the default shader file path:
		std::string const& shaderDir = 
			core::Config::Get()->GetValue<std::string>(core::configkeys::k_shaderDirectoryKey);
		std::string filepath = shaderDir + filenameAndExtension;

		// Attempt to load the shader
		std::string shaderText = util::LoadTextAsString(filepath);

		bool foundText = !shaderText.empty();

		// If loading failed, check the additional search locations:
		if (!foundText)
		{
			constexpr std::array<char const*, 1> k_additionalSearchDirs =
			{
				core::configkeys::k_commonShaderDirName,
			};

			for (size_t i = 0; i < k_additionalSearchDirs.size(); i++)
			{
				filepath = k_additionalSearchDirs[i] + filenameAndExtension;

				shaderText = util::LoadTextAsString(filepath);
				if (!shaderText.empty())
				{
					foundText = true;
					break;
				}
			}
		}

		if (foundText)
		{
			return std::format(
				"//--------------------------------------------------------------------------------------\n"
				"// {}:\n"
				"//--------------------------------------------------------------------------------------\n"
				"{}",
				filenameAndExtension,
				shaderText);
		}

		return shaderText;
	}


	std::string LoadShaderText(std::string const& filename, re::Shader::ShaderType shaderType)
	{
		std::string const& filenameAndExtension = filename + k_shaderFileExtensions[shaderType];

		return LoadShaderText(filenameAndExtension);
	}


	std::vector<std::future<void>> LoadShaderTexts(
		std::vector<std::pair<std::string, re::Shader::ShaderType>>const& extensionlessSourceFilenames,
		std::array<std::string, re::Shader::ShaderType_Count>& shaderTextsOut)
	{
		std::vector<std::future<void>> taskFutures;
		taskFutures.reserve(re::Shader::ShaderType_Count);

		for (auto const& source : extensionlessSourceFilenames)
		{
			std::string const& filename = source.first;
			const re::Shader::ShaderType shaderType = source.second;

			taskFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
				[&shaderTextsOut, filename, shaderType]()
				{
					shaderTextsOut[shaderType] = LoadShaderText(filename, shaderType);
				}));
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
			if (includeStartIdx != std::string::npos)
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
				if (includeEndIndex != std::string::npos)
				{
					size_t firstQuoteIndex, lastQuoteIndex;

					firstQuoteIndex = shaderText.find("\"", includeStartIdx + 1);
					if (firstQuoteIndex != std::string::npos && 
						firstQuoteIndex > 0 && 
						firstQuoteIndex < includeEndIndex)
					{
						lastQuoteIndex = shaderText.find("\"", firstQuoteIndex + 1);
						if (lastQuoteIndex != std::string::npos && 
							lastQuoteIndex > firstQuoteIndex && 
							lastQuoteIndex < includeEndIndex)
						{
							firstQuoteIndex++; // Move ahead 1 element from the first quotation mark

							const size_t includeFileNameStrLength = lastQuoteIndex - firstQuoteIndex;
							std::string const& includeFileName = 
								shaderText.substr(firstQuoteIndex, includeFileNameStrLength);

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
		} while (includeStartIdx != std::string::npos && includeStartIdx < shaderText.length());

		// Insert the last block
		if (blockStartIdx < shaderText.size())
		{
			shaderTextStrings.emplace_back(shaderText.substr(blockStartIdx, std::string::npos));
		}
		return true;
	}


	void DebugOutputShaderText(std::string const& shaderTextFilename, std::vector<GLchar const*> const& shaderTextStrings)
	{
		constexpr char const* k_outputDir = ".\\Shaders\\GLSL\\Debug\\";
		std::filesystem::create_directory(k_outputDir); // No error if the directory already exists

		std::ofstream shaderTextOutStream;

		std::string const& outputFilepath = k_outputDir + shaderTextFilename;
		shaderTextOutStream.open(outputFilepath.c_str(), std::ios::out);
		SEAssert(shaderTextOutStream.good(), "Error creating shader debug output stream");

		for (auto const& shaderTextStr : shaderTextStrings)
		{
			shaderTextOutStream << shaderTextStr;
			shaderTextOutStream << "\n";
		}

		shaderTextOutStream.close();
	}
}


namespace opengl
{
	void Shader::Create(re::Shader& shader)
	{
		util::PerformanceTimer timer;
		timer.Start();

		opengl::Shader::PlatformParams* params = shader.GetPlatformParams()->As<opengl::Shader::PlatformParams*>();

		SEAssert(!params->m_isCreated, "Shader has already been created");
		params->m_isCreated = true;

		std::string const& shaderFileName = shader.GetName();
		LOG("Creating shader: \"%s\"", shaderFileName.c_str());

		// Load the individual .vert/.frag/etc shader text files:
		std::vector<std::future<void>> const& loadShaderTextsTaskFutures = 
			LoadShaderTexts(shader.m_extensionlessSourceFilenames, params->m_shaderTexts);

		// Load the shaders, and assemble params we'll need soon:
		std::array<std::string, re::Shader::ShaderType_Count> shaderFiles;
		std::array<std::string, re::Shader::ShaderType_Count> shaderFileNames; // For RenderDoc markers

		// Each shader type (.vert/.frag etc) is loaded as a vector of substrings
		std::array<std::vector<std::string>, re::Shader::ShaderType_Count> shaderTextStrings;

		// Figure out what type of shader(s) we're loading:
		std::array<uint32_t, re::Shader::ShaderType_Count> foundShaderTypeFlags{0};

		// Make sure we're done loading the shader texts before we continue:
		for (auto const& loadFuture : loadShaderTextsTaskFutures)
		{
			loadFuture.wait();
		}

		// Pre-process the shader text:
		std::array< std::future<void>, re::Shader::ShaderType_Count> processIncludesTaskFutures;
		for (size_t i = 0; i < re::Shader::ShaderType_Count; i++)
		{
			if (!params->m_shaderTexts[i].empty())
			{
				foundShaderTypeFlags[i] = k_shaderTypeFlags[i]; // Mark the shader as seen
				shaderFiles[i] = std::move(params->m_shaderTexts[i]); // Move the shader texts, they're no longer needed
				shaderFileNames[i] = shaderFileName + k_shaderFileExtensions[i];

				// Queue a job to parse the #include text:
				processIncludesTaskFutures[i] = core::ThreadPool::Get()->EnqueueJob(
					[&shaderFileNames, &shaderFiles, &shaderTextStrings, i]()
					{
						const bool result = InsertIncludeText(shaderFiles[i], shaderTextStrings[i]);
						SEAssert(result, "Failed to parse shader #includes");
					});
			}
		}
		SEAssert(foundShaderTypeFlags[re::Shader::Vertex] != 0 || foundShaderTypeFlags[re::Shader::Compute] != 0,
			"No shader found. Must have a vertex or compute shader at minimum");

		SEAssert(foundShaderTypeFlags[re::Shader::Mesh] == 0 && foundShaderTypeFlags[re::Shader::Amplification] == 0,
			"Mesh and amplification shaders are currently only supported via an NVidia extension (and not on AMD). For"
			"now, we don't support them.");

		// Static so we only compute this once
		static const size_t preambleLength = strlen(k_globalPreamble);

		// Create an empty shader program object:
		params->m_shaderReference = glCreateProgram();

		const bool debugShaderResult = core::Config::Get()->KeyExists("debugopenglshaderpreprocessing");

		// Create and attach the shader stages:
		for (size_t i = 0; i < shaderFiles.size(); i++)
		{
			if (foundShaderTypeFlags[i] == 0)
			{
				continue;
			}

			// Create and attach the shader object:
			const GLuint shaderObject = glCreateShader(foundShaderTypeFlags[i]);
			SEAssert(shaderObject > 0, "glCreateShader failed!");

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

			if (debugShaderResult)
			{
				DebugOutputShaderText(shaderFileNames[i], shaderSourceStrings);
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
		std::vector<GLchar> name(maxUniformNameLength, '\0'); // Uniform name, as described in the shader text
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

			if (UniformIsSamplerType(type))
			{
				// Get the texture unit binding value:
				GLint val;
				glGetUniformiv(params->m_shaderReference, (GLuint)i, &val);

				// Populate the shader sampler unit map with unique entries:
				std::string nameStr(name.data());
				SEAssert(params->m_samplerUnits.find(nameStr) == params->m_samplerUnits.end(),
					"Sampler unit already found! Does the shader have a unique binding layout qualifier?");

				params->m_samplerUnits.emplace(std::move(nameStr), static_cast<int32_t>(val));
			}
		}

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
		std::string const& uniformName,
		void const* value, 
		opengl::Shader::UniformType const type, 
		int const count)
	{
		PlatformParams const* params = shader.GetPlatformParams()->As<opengl::Shader::PlatformParams const*>();
		SEAssert(params->m_isCreated == true, "Shader has not been created yet");

		GLuint uniformID = glGetUniformLocation(params->m_shaderReference, uniformName.c_str());

		switch (type)
		{
		case opengl::Shader::UniformType::Matrix4x4f:
		{
			glUniformMatrix4fv(uniformID, count, GL_FALSE, static_cast<GLfloat const*>(value));
		}
		break;
		case opengl::Shader::UniformType::Matrix3x3f:
		{
			glUniformMatrix3fv(uniformID, count, GL_FALSE, static_cast<GLfloat const*>(value));
		}
		break;
		case opengl::Shader::UniformType::Vec3f:
		{
			glUniform3fv(uniformID, count, static_cast<GLfloat const*>(value));
		}
		break;
		case opengl::Shader::UniformType::Vec4f:
		{
			glUniform4fv(uniformID, count, static_cast<GLfloat const*>(value));
		}
		break;
		case opengl::Shader::UniformType::Float:
		{
			glUniform1f(uniformID, *static_cast<GLfloat const*>(value));
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
				SEAssert(core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
					std::format("Shader \"{}\" texture name \"{}\"is invalid, and strict shader binding is enabled", 
						shader.GetName(), uniformName).c_str());
				return;
			}

			opengl::Texture::Bind(*static_cast<re::Texture const*>(value), bindingUnit->second);
		}
		break;
		case opengl::Shader::UniformType::Sampler:
		{
			auto const& bindingUnit = params->m_samplerUnits.find(uniformName);

			if (bindingUnit == params->m_samplerUnits.end())
			{
				SEAssert(core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
					std::format("Shader \"{}\" sampler name \"{}\"is invalid, and strict shader binding is enabled", 
						shader.GetName(), uniformName).c_str());
				return;
			}

			opengl::Sampler::Bind(*static_cast<re::Sampler const*>(value), bindingUnit->second);
		}
		break;
		default:
			SEAssertF("Shader uniform upload failed: Recieved unimplemented uniform type");
		}
	}


	void Shader::SetBuffer(re::Shader const& shader, re::Buffer const& buffer)
	{
		opengl::Shader::PlatformParams const* shaderPlatformParams = 
			shader.GetPlatformParams()->As<opengl::Shader::PlatformParams const*>();

		SEAssert(shaderPlatformParams->m_isCreated == true, "Shader has not been created yet");
		
		GLint bindIndex = 0;
		const GLenum properties = GL_BUFFER_BINDING;

		re::Buffer::PlatformParams const* bufferPlatformParams = buffer.GetPlatformParams();
		switch (buffer.GetBufferParams().m_dataType)
		{
		case re::Buffer::DataType::Constant: // Bind our single-element buffers as UBOs
		{
			// Find the buffer binding index via introspection
			const GLint uniformBlockIdx = glGetProgramResourceIndex(
				shaderPlatformParams->m_shaderReference,	// program
				GL_UNIFORM_BLOCK,							// programInterface
				buffer.GetName().c_str());					// name

			SEAssert(uniformBlockIdx != GL_INVALID_ENUM, "Failed to get resource index");

			// GL_INVALID_INDEX is returned if the the uniform block name does not identify an active uniform block
			SEAssert(uniformBlockIdx != GL_INVALID_INDEX ||
				core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
				"Failed to find an active uniform block index. This is is not an error, but a useful debugging helper");

			if (uniformBlockIdx != GL_INVALID_INDEX)
			{
				glGetProgramResourceiv(
					shaderPlatformParams->m_shaderReference,
					GL_UNIFORM_BLOCK,
					uniformBlockIdx,
					1,
					&properties,
					1,
					NULL,
					&bindIndex);
			}
		}
		break;
		case re::Buffer::DataType::Structured: // Bind our array buffers as SSBOs, as they support dynamic indexing
		{
			// Find the buffer binding index via introspection
			const GLint ssboIdx = glGetProgramResourceIndex(
			shaderPlatformParams->m_shaderReference,	// program
			GL_SHADER_STORAGE_BLOCK,					// programInterface
			buffer.GetName().c_str());					// name

			SEAssert(ssboIdx != GL_INVALID_ENUM, "Failed to get resource index");

			// GL_INVALID_INDEX is returned if name is not the name of a resource within the shader program
			SEAssert(ssboIdx != GL_INVALID_INDEX ||
				core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
				"Failed to find the resource in the shader. This is is not an error, but a useful debugging helper");

			if (ssboIdx != GL_INVALID_INDEX)
			{
				glGetProgramResourceiv(
					shaderPlatformParams->m_shaderReference,
					GL_SHADER_STORAGE_BLOCK,
					ssboIdx,
					1,
					&properties,
					1,
					NULL,
					&bindIndex);
			}
		}
		break;
		default: SEAssertF("Invalid DataType");
		}

		// Bind our buffer to the retrieved bind index:
		opengl::Buffer::Bind(buffer, bindIndex);
	}


	void Shader::SetTextureAndSampler(re::Shader const& shader, re::TextureAndSamplerInput const& texSamplerInput)
	{
		PlatformParams const* params = shader.GetPlatformParams()->As<opengl::Shader::PlatformParams const*>();
		SEAssert(params->m_isCreated == true, "Shader has not been created yet");

		// Bind the texture:
		auto const& textureBindingUnit = params->m_samplerUnits.find(texSamplerInput.m_shaderName);
		if (textureBindingUnit == params->m_samplerUnits.end())
		{
			
			SEAssert(core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
				std::format("Shader \"{}\" texture name \"{}\"is invalid, and strict shader binding is enabled",
					shader.GetName(), texSamplerInput.m_shaderName).c_str());
			return;
		}
		opengl::Texture::Bind(*texSamplerInput.m_texture, textureBindingUnit->second, texSamplerInput.m_textureView);


		// Bind the sampler:
		auto const& samplerBindingUnit = params->m_samplerUnits.find(texSamplerInput.m_shaderName);
		if (samplerBindingUnit == params->m_samplerUnits.end())
		{
			SEAssert(core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
				std::format("Shader \"{}\" sampler name \"{}\"is invalid, and strict shader binding is enabled",
					shader.GetName(), texSamplerInput.m_shaderName).c_str());
			return;
		}
		opengl::Sampler::Bind(*texSamplerInput.m_sampler, samplerBindingUnit->second);
	}
}