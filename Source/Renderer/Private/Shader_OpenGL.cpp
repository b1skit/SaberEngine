// © 2022 Adam Badke. All rights reserved.
#include "Private/Batch.h"
#include "Private/Buffer_OpenGL.h"
#include "Private/RootConstants.h"
#include "Private/Sampler_OpenGL.h"
#include "Private/Shader.h"
#include "Private/Shader_OpenGL.h"
#include "Private/Texture_OpenGL.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/ThreadPool.h"

#include "Core/Host/PerformanceTimer.h"

#include "Core/Util/TextUtils.h"

#include <GL/glew.h> 


// Enable this to see names/indexes; A convenience since we use StringHash as a key
//#define LOG_SHADER_NAMES


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

		GL_COMPUTE_SHADER,

		// Entires corresponding to ray tracing shader types included for consistency, but not ever used in OpenGL
		GL_INVALID_ENUM, // ShaderType::HitGroup_Intersection
		GL_INVALID_ENUM, // ShaderType::HitGroup_AnyHit
		GL_INVALID_ENUM, // ShaderType::HitGroup_ClosestHit
		GL_INVALID_ENUM, // ShaderType::Callable
		GL_INVALID_ENUM, // ShaderType::RayGen
		GL_INVALID_ENUM, // ShaderType::Miss
	};
	static_assert(_countof(k_shaderTypeFlags) == re::Shader::ShaderType_Count);

	bool UniformIsSamplerType(GLenum type)
	{
		switch(type)
		{
			// GL_VERSION_2_0:
			case GL_SAMPLER_1D:
			case GL_SAMPLER_2D:
			case GL_SAMPLER_3D:
			case GL_SAMPLER_CUBE:
			case GL_SAMPLER_1D_SHADOW:
			case GL_SAMPLER_2D_SHADOW:
			// GL_VERSION_3_0:
			case GL_SAMPLER_1D_ARRAY:
			case GL_SAMPLER_2D_ARRAY:
			case GL_SAMPLER_1D_ARRAY_SHADOW:
			case GL_SAMPLER_2D_ARRAY_SHADOW:
			case GL_SAMPLER_CUBE_SHADOW:
			case GL_INT_SAMPLER_1D:
			case GL_INT_SAMPLER_2D:
			case GL_INT_SAMPLER_3D:
			case GL_INT_SAMPLER_CUBE:
			case GL_INT_SAMPLER_1D_ARRAY:
			case GL_INT_SAMPLER_2D_ARRAY:
			case GL_UNSIGNED_INT_SAMPLER_1D:
			case GL_UNSIGNED_INT_SAMPLER_2D:
			case GL_UNSIGNED_INT_SAMPLER_3D:
			case GL_UNSIGNED_INT_SAMPLER_CUBE:
			case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
			case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
			// GL_VERSION_3_1:
			case GL_SAMPLER_2D_RECT:
			case GL_SAMPLER_2D_RECT_SHADOW:
			case GL_SAMPLER_BUFFER:
			case GL_INT_SAMPLER_2D_RECT:
			case GL_INT_SAMPLER_BUFFER:
			case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
			case GL_UNSIGNED_INT_SAMPLER_BUFFER:
			// GL_VERSION_4_0:
			case GL_SAMPLER_CUBE_MAP_ARRAY:
			case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
			case GL_INT_SAMPLER_CUBE_MAP_ARRAY:
			case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:
			// GL_ARB_texture_multisample
			case GL_SAMPLER_2D_MULTISAMPLE:
			case GL_INT_SAMPLER_2D_MULTISAMPLE:
			case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
			case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
			case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
			case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
			// GL_ARB_shader_image_load_store
			case GL_IMAGE_1D:
			case GL_IMAGE_2D:
			case GL_IMAGE_3D:
			case GL_IMAGE_2D_RECT:
			case GL_IMAGE_CUBE:
			case GL_IMAGE_BUFFER:
			case GL_IMAGE_1D_ARRAY:
			case GL_IMAGE_2D_ARRAY:
			case GL_IMAGE_CUBE_MAP_ARRAY:
			case GL_IMAGE_2D_MULTISAMPLE:
			case GL_IMAGE_2D_MULTISAMPLE_ARRAY:
			case GL_INT_IMAGE_1D:
			case GL_INT_IMAGE_2D:
			case GL_INT_IMAGE_3D:
			case GL_INT_IMAGE_2D_RECT:
			case GL_INT_IMAGE_CUBE:
			case GL_INT_IMAGE_BUFFER:
			case GL_INT_IMAGE_1D_ARRAY:
			case GL_INT_IMAGE_2D_ARRAY:
			case GL_INT_IMAGE_CUBE_MAP_ARRAY:
			case GL_INT_IMAGE_2D_MULTISAMPLE:
			case GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
			case GL_UNSIGNED_INT_IMAGE_1D:
			case GL_UNSIGNED_INT_IMAGE_2D:
			case GL_UNSIGNED_INT_IMAGE_3D:
			case GL_UNSIGNED_INT_IMAGE_2D_RECT:
			case GL_UNSIGNED_INT_IMAGE_CUBE:
			case GL_UNSIGNED_INT_IMAGE_BUFFER:
			case GL_UNSIGNED_INT_IMAGE_1D_ARRAY:
			case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
			case GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:
			case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
			case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
				return true;
			default: return false;
		}
	}


	void AssertShaderIsValid(
		std::string const& shaderName, uint32_t shaderRef, uint32_t flag, bool isProgram)
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
			constexpr std::array<char const*, 2> k_additionalSearchDirs =
			{
				core::configkeys::k_commonShaderDirName,
				core::configkeys::k_generatedGLSLShaderDirName,
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

		return shaderText;
	}


	std::vector<std::future<void>> LoadShaderTexts(
		std::vector<re::Shader::Metadata>const& metadata,
		std::array<std::string, re::Shader::ShaderType_Count>& shaderTextsOut)
	{
		std::vector<std::future<void>> taskFutures;
		taskFutures.reserve(re::Shader::ShaderType_Count);

		for (auto const& source : metadata)
		{
			std::string const& filename = source.m_extensionlessFilename;
			const re::Shader::ShaderType shaderType = source.m_type;

			taskFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
				[&shaderTextsOut, filename, shaderType]()
				{
					std::string const& filenameAndExtension = filename + ".glsl";

					shaderTextsOut[shaderType] = LoadShaderText(filenameAndExtension);
				}));
		}

		return taskFutures;
	}

	
	// OpenGL shader reflection reports buffer array names with their index prefix tokes (E.g. MyBuf[0], MyBug[1], etc).
	// This strips those out, and gives us the index they contained
	std::string StripArrayTokens(std::string const& name, GLint& arrayIdxOut)
	{
		arrayIdxOut = 0;

		const size_t openArrayBraceIdx = name.find_first_of('[');
		if (openArrayBraceIdx == std::string::npos)
		{
			return name;
		}

		arrayIdxOut = std::stoi(name.substr(openArrayBraceIdx + 1, name.length() - openArrayBraceIdx + 1));

		return name.substr(0, openArrayBraceIdx);
	}


	void BuildShaderReflection(re::Shader const& shader)
	{
#if defined(LOG_SHADER_NAMES)
		LOG("Building shader reflection for shader \"%s\"", shader.GetName().c_str());
#endif

		opengl::Shader::PlatObj* platObj = shader.GetPlatformObject()->As<opengl::Shader::PlatObj*>();

		// Populate the uniform locations
		// Get the number of active uniforms found in the shader:
		GLint numUniforms = 0;
		glGetProgramiv(platObj->m_shaderReference, GL_ACTIVE_UNIFORMS, &numUniforms);

		// Get the max length of the active uniform names found in the shader:
		GLint maxUniformNameLength = 0;
		glGetProgramiv(platObj->m_shaderReference, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformNameLength);

		// Sampler uniforms:
		GLint uniformSize = 0; // Size of the uniform variable; currently we just ignore this
		GLenum uniformType; // Data type of the uniform
		std::vector<GLchar> samplerName(maxUniformNameLength, '\0'); // Uniform name, as described in the shader text
		for (GLint uniformIdx = 0; uniformIdx < numUniforms; uniformIdx++)
		{
			// Get the size, type, and name of the uniform at the current index
			glGetActiveUniform(
				platObj->m_shaderReference,		// program
				static_cast<GLuint>(uniformIdx),	// index
				maxUniformNameLength,				// buffer size
				nullptr,							// length
				&uniformSize,						// size
				&uniformType,						// type
				samplerName.data());				// name

			if (UniformIsSamplerType(uniformType))
			{
				const GLuint uniformLocation = glGetUniformLocation(platObj->m_shaderReference, samplerName.data());

				// Get the texture unit binding value:
				GLint bindIdx = 0;
				glGetUniformiv(
					platObj->m_shaderReference,	// program
					uniformLocation,				// location
					&bindIdx);						// params

				// Populate the shader sampler unit map with unique entries:
				std::string nameStr(samplerName.data());
				SEAssert(platObj->m_samplerUnits.find(nameStr) == platObj->m_samplerUnits.end(),
					"Sampler unit already found! Does the shader have a unique binding layout qualifier?");

				platObj->m_samplerUnits.emplace(std::move(nameStr), bindIdx);

#if defined(LOG_SHADER_NAMES)
				LOG("Shader \"%s\": Found sampler uniform %s = %d",
					shader.GetName().c_str(), samplerName.data(), bindIdx);
#endif
			}
		}

		// Vertex attributes:
		GLint numAttributes = 0;
		glGetProgramiv(platObj->m_shaderReference, GL_ACTIVE_ATTRIBUTES, &numAttributes);

		GLint maxAttributeNameLength = 0;
		glGetProgramiv(platObj->m_shaderReference, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxAttributeNameLength);

		GLint attributeSize = 0; // Size of the uniform variable; currently we just ignore this
		GLenum attributeType; // Data type of the uniform
		std::vector<GLchar> attributeName(maxAttributeNameLength, '\0'); // Attribute name, as described in the shader text

		for (GLint attributeIdx = 0; attributeIdx < numAttributes; attributeIdx++)
		{
			glGetActiveAttrib(
				platObj->m_shaderReference,		// program
				static_cast<GLuint>(attributeIdx),	// index
				maxAttributeNameLength,				// buffer size
				nullptr,							// length
				&attributeSize,						// size
				&attributeType,						// type
				attributeName.data());				// name

			const GLint attributeLocation = glGetAttribLocation(platObj->m_shaderReference, attributeName.data());

			if (attributeLocation >= 0) // -1 for gl_InstanceID, gl_VertexID etc
			{
				platObj->m_vertexAttributeLocations.emplace(attributeName.data(), attributeLocation);

#if defined(LOG_SHADER_NAMES)
				LOG("Shader \"%s\": Found vertex attribute %s = %d",
					shader.GetName().c_str(), attributeName.data(), attributeLocation);
#endif
			}
		}

		constexpr size_t k_maxResourceNameLength = 512;
		std::vector<GLchar> resourceName(k_maxResourceNameLength, '\0');

		constexpr GLenum k_bufferProperty = GL_BUFFER_BINDING;

		// UBOs:
		GLint numActiveUniformBlocks = 0;
		glGetProgramInterfaceiv(
			platObj->m_shaderReference, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &numActiveUniformBlocks);

		for (GLint uboIdx = 0; uboIdx < numActiveUniformBlocks; ++uboIdx)
		{
			glGetProgramResourceName(
				platObj->m_shaderReference,					// program
				GL_UNIFORM_BLOCK,								// programInterface
				static_cast<GLuint>(uboIdx),			// index
				static_cast<GLsizei>(k_maxResourceNameLength),	// bufSize
				nullptr,										// length (optional)
				resourceName.data());							// name

			GLint uboBindIdx = 0;

			glGetProgramResourceiv(
				platObj->m_shaderReference,
				GL_UNIFORM_BLOCK,
				uboIdx,
				1,
				&k_bufferProperty,
				1,
				NULL,
				&uboBindIdx);
			SEAssert(uboBindIdx >= 0, "Invalid buffer bind index returned");

			constexpr opengl::Buffer::BindTarget k_bindTarget = opengl::Buffer::BindTarget::UBO;

			platObj->AddBufferMetadata(resourceName.data(), k_bindTarget, uboBindIdx);

#if defined(LOG_SHADER_NAMES)
			LOG("Shader \"%s\": Found UBO %s = %d", shader.GetName().c_str(), resourceName.data(), uboBindIdx);
#endif
		}

		// SSBOs:
		GLint numActiveShaderStorageBlocks = 0;
		glGetProgramInterfaceiv(
			platObj->m_shaderReference, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &numActiveShaderStorageBlocks);

		for (GLint ssboIdx = 0; ssboIdx < numActiveShaderStorageBlocks; ++ssboIdx)
		{
			glGetProgramResourceName(
				platObj->m_shaderReference,					// program
				GL_SHADER_STORAGE_BLOCK,						// programInterface
				static_cast<GLuint>(ssboIdx),					// index
				static_cast<GLsizei>(k_maxResourceNameLength),	// bufSize
				nullptr,										// length (optional)
				resourceName.data());							// name

			GLint storageBlockBindIdx = 0;

			glGetProgramResourceiv(
				platObj->m_shaderReference,
				GL_SHADER_STORAGE_BLOCK,
				ssboIdx,
				1,
				&k_bufferProperty,
				1,
				NULL,
				&storageBlockBindIdx);

			constexpr opengl::Buffer::BindTarget k_bindTarget = opengl::Buffer::BindTarget::SSBO;

			platObj->AddBufferMetadata(resourceName.data(), k_bindTarget, storageBlockBindIdx);

#if defined(LOG_SHADER_NAMES)
			LOG("Shader \"%s\": Found SSBO %s = %d", shader.GetName().c_str(), resourceName.data(), storageBlockBindIdx);
#endif
		}
	}


	constexpr opengl::Shader::UniformType DataTypeToUniformType(re::DataType dataType)
	{
		switch (dataType)
		{
		case re::DataType::Float: return opengl::Shader::UniformType::Float;
		case re::DataType::Float2: return opengl::Shader::UniformType::Vec2f;
		case re::DataType::Float3: return opengl::Shader::UniformType::Vec3f;
		case re::DataType::Float4: return opengl::Shader::UniformType::Vec4f;

		case re::DataType::Int:	return opengl::Shader::UniformType::Int;
		case re::DataType::Int2: return opengl::Shader::UniformType::Int2;
		case re::DataType::Int3: return opengl::Shader::UniformType::Int3;
		case re::DataType::Int4: return opengl::Shader::UniformType::Int4;

		case re::DataType::UInt: return opengl::Shader::UniformType::UInt;
		case re::DataType::UInt2: return opengl::Shader::UniformType::UInt2;
		case re::DataType::UInt3: return opengl::Shader::UniformType::UInt3;
		case re::DataType::UInt4: return opengl::Shader::UniformType::UInt4;

		default: SEAssertF("Invalid/unsupported data type for con");
		}
		return opengl::Shader::UniformType::UInt; // This should never happen
	}
}


namespace opengl
{
	void opengl::Shader::PlatObj::AddBufferMetadata(
		char const* name, opengl::Buffer::BindTarget bindTarget, GLint bufferLocation)
	{
		constexpr GLint k_invalidLocationIdx = -1;

		// Parse the reflected buffer name and index:
		GLint arrayIdx = 0;
		std::string const& strippedName = StripArrayTokens(name, arrayIdx);

		util::HashKey const& strippedNameHash = util::HashKey(strippedName);
		if (m_bufferMetadata.contains(strippedNameHash))
		{
			SEAssert(m_bufferMetadata.at(strippedNameHash).m_bindTarget == bindTarget,
				"Found an existing entry with a different bind target. This is unexpected");

			std::vector<GLint>& bufLocations = m_bufferMetadata.at(strippedNameHash).m_bufferLocations;

			if (arrayIdx >= bufLocations.size())
			{
				bufLocations.resize(arrayIdx + 1, k_invalidLocationIdx);
			}

			bufLocations[arrayIdx] = bufferLocation;
		}
		else
		{
			std::vector<GLint> bufLocations(arrayIdx + 1, k_invalidLocationIdx);
			bufLocations[arrayIdx] = bufferLocation;

			m_bufferMetadata.emplace(strippedNameHash,
				opengl::Shader::PlatObj::BufferMetadata{
				.m_bindTarget = bindTarget,
				.m_bufferLocations = std::move(bufLocations),
				});
		}
	}


	void Shader::Create(re::Shader& shader)
	{
		host::PerformanceTimer timer;
		timer.Start();

		opengl::Shader::PlatObj* platObj = shader.GetPlatformObject()->As<opengl::Shader::PlatObj*>();

		SEAssert(!platObj->m_isCreated, "Shader has already been created");
		platObj->m_isCreated = true;

		std::string const& shaderFileName = shader.GetName();
		LOG("Creating shader: \"%s\"", shaderFileName.c_str());

		// Load the individual shader text files:
		SEAssert(!shader.m_metadata.empty(), "Shader does not contain any metadata");
		std::vector<std::future<void>> const& loadShaderTextsTaskFutures = 
			LoadShaderTexts(shader.m_metadata, platObj->m_shaderTexts);

		// Load the shaders, and assemble params we'll need soon:
		std::array<std::string, re::Shader::ShaderType_Count> shaderFiles;
		std::array<std::string, re::Shader::ShaderType_Count> shaderFileNames; // For RenderDoc markers

		// Figure out what type of shader(s) we're loading:
		std::array<uint32_t, re::Shader::ShaderType_Count> foundShaderTypeFlags{0};

		// Make sure we're done loading the shader texts before we continue:
		for (auto const& loadFuture : loadShaderTextsTaskFutures)
		{
			loadFuture.wait();
		}

		// Determine which shaders we've loaded:
		for (size_t i = 0; i < re::Shader::ShaderType_Count; i++)
		{
			if (!platObj->m_shaderTexts[i].empty())
			{
				foundShaderTypeFlags[i] = k_shaderTypeFlags[i]; // Mark the shader as seen
				shaderFiles[i] = std::move(platObj->m_shaderTexts[i]); // Move the shader texts, they're no longer needed
				shaderFileNames[i] = shaderFileName + ".glsl";
			}
		}
		SEAssert(foundShaderTypeFlags[re::Shader::Vertex] != 0 || foundShaderTypeFlags[re::Shader::Compute] != 0,
			"No shader found. Must have a vertex or compute shader at minimum");

		SEAssert(foundShaderTypeFlags[re::Shader::Mesh] == 0 && foundShaderTypeFlags[re::Shader::Amplification] == 0,
			"Mesh and amplification shaders are currently only supported via an NVidia extension (and not on AMD). For"
			"now, we don't support them.");

		// Create an empty shader program object:
		platObj->m_shaderReference = glCreateProgram();

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

			// Build our list of shader string pointers for compilation:
			std::vector<GLchar const*> shaderSourceStrings;
			shaderSourceStrings.emplace_back(shaderFiles[i].c_str());

			std::vector<GLint> shaderSourceStringLengths;
			shaderSourceStringLengths.emplace_back(static_cast<GLint>(shaderFiles[i].length()));

			// Attach the shader text:			
			glShaderSource(
				shaderObject,
				static_cast<GLsizei>(shaderSourceStrings.size()),
				shaderSourceStrings.data(),
				shaderSourceStringLengths.data());

			glCompileShader(shaderObject);

			AssertShaderIsValid(shader.GetName(), shaderObject, GL_COMPILE_STATUS, false/*= isProgram*/);

			glAttachShader(platObj->m_shaderReference, shaderObject); // Attach our shaders to the shader program

			// Delete the shader stage now that we've attached it
			glDeleteShader(shaderObject);
		}

		// Link our program object:
		glLinkProgram(platObj->m_shaderReference);
		AssertShaderIsValid(shader.GetName(), platObj->m_shaderReference, GL_LINK_STATUS, true/*= isProgram*/);

		// Validate our program objects can execute with our current OpenGL state:
		glValidateProgram(platObj->m_shaderReference);
		AssertShaderIsValid(shader.GetName(), platObj->m_shaderReference, GL_VALIDATE_STATUS, true/*= isProgram*/);

		BuildShaderReflection(shader);

		LOG("Shader \"%s\" created in %f seconds", shaderFileName.c_str(), timer.StopSec());
	}


	void Shader::Destroy(re::Shader& shader)
	{
		PlatObj* platObj = shader.GetPlatformObject()->As<opengl::Shader::PlatObj*>();
		if (!platObj->m_isCreated)
		{
			return;
		}
		platObj->m_isCreated = false;

		glDeleteProgram(platObj->m_shaderReference);
		platObj->m_shaderReference = 0;
		glUseProgram(0); // Unbind, as glGetIntegerv(GL_CURRENT_PROGRAM, shaderRef) still returns the shader ref otherwise
	}


	void Shader::Bind(re::Shader const& shader)
	{
		opengl::Shader::PlatObj const* platObj = 
			shader.GetPlatformObject()->As<opengl::Shader::PlatObj const*>();

		glUseProgram(platObj->m_shaderReference);
	}


	void Shader::SetRootConstants(re::Shader const& shader, re::RootConstants const& rootConstants)
	{
		for (uint8_t i = 0; i < rootConstants.GetRootConstantCount(); ++i)
		{
			const Shader::UniformType uniformType = DataTypeToUniformType(rootConstants.GetDataType(i));

			void const* value = rootConstants.GetValue(i);
			opengl::Shader::SetUniform(shader, rootConstants.GetShaderName(i), value, uniformType, 1);
		}
	}


	void Shader::SetUniform(
		re::Shader const& shader,
		std::string const& uniformName,
		void const* value, 
		opengl::Shader::UniformType const type, 
		int count)
	{
		opengl::Shader::PlatObj const* platObj = 
			shader.GetPlatformObject()->As<opengl::Shader::PlatObj const*>();
		SEAssert(platObj->m_isCreated == true, "Shader has not been created yet");

		GLuint uniformID = glGetUniformLocation(platObj->m_shaderReference, uniformName.c_str());

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
		case opengl::Shader::UniformType::Float:
		{
			glUniform1fv(uniformID, count, static_cast<GLfloat const*>(value));
		}
		break;
		case opengl::Shader::UniformType::Vec2f:
		{
			glUniform2fv(uniformID, count, static_cast<GLfloat const*>(value));
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
		case opengl::Shader::UniformType::Int:
		{
			glUniform1iv(uniformID, count, static_cast<GLint const*>(value));
		}
		break;
		case opengl::Shader::UniformType::Int2:
		{
			glUniform2iv(uniformID, count, static_cast<GLint const*>(value));
		}
		break;
		case opengl::Shader::UniformType::Int3:
		{
			glUniform3iv(uniformID, count, static_cast<GLint const*>(value));
		}
		break;
		case opengl::Shader::UniformType::Int4:
		{
			glUniform4iv(uniformID, count, static_cast<GLint const*>(value));
		}
		break;
		case opengl::Shader::UniformType::UInt:
		{
			glUniform1uiv(uniformID, count, static_cast<GLuint const*>(value));
		}
		break;
		case opengl::Shader::UniformType::UInt2:
		{
			glUniform2uiv(uniformID, count, static_cast<GLuint const*>(value));
		}
		break;
		case opengl::Shader::UniformType::UInt3:
		{
			glUniform3uiv(uniformID, count, static_cast<GLuint const*>(value));
		}
		break;
		case opengl::Shader::UniformType::UInt4:
		{
			glUniform4uiv(uniformID, count, static_cast<GLuint const*>(value));
		}
		break;

		//case opengl::Shader::UniformType::Texture:
		//{
		//	//auto const& bindingUnit = platObj->m_samplerUnits.find(uniformName);
		//	//if (bindingUnit == platObj->m_samplerUnits.end())
		//	//{
		//	//	SEAssert(core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
		//	//		std::format("Shader \"{}\" texture name \"{}\"is invalid, and strict shader binding is enabled", 
		//	//			shader.GetName(), uniformName).c_str());
		//	//	return;
		//	//}

		//	//opengl::Texture::Bind(*static_cast<re::Texture const*>(value), bindingUnit->second);
		//}
		//break;
		//case opengl::Shader::UniformType::Sampler:
		//{
		//	auto const& bindingUnit = platObj->m_samplerUnits.find(uniformName);

		//	if (bindingUnit == platObj->m_samplerUnits.end())
		//	{
		//		SEAssert(core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
		//			std::format("Shader \"{}\" sampler name \"{}\"is invalid, and strict shader binding is enabled", 
		//				shader.GetName(), uniformName).c_str());
		//		return;
		//	}

		//	opengl::Sampler::Bind(*static_cast<re::Sampler const*>(value), bindingUnit->second);
		//}
		//break;
		default: SEAssertF("Invalid uniform type");
		}
	}


	void Shader::SetBuffer(re::Shader const& shader, re::BufferInput const& bufferInput)
	{
		opengl::Shader::PlatObj const* shaderPlatObj = 
			shader.GetPlatformObject()->As<opengl::Shader::PlatObj const*>();

		SEAssert(shaderPlatObj->m_isCreated == true, "Shader has not been created yet");
		
		re::Buffer::PlatObj const* bufferPlatformParams = bufferInput.GetBuffer()->GetPlatformObject();

		SEAssert(shaderPlatObj->m_bufferMetadata.contains(bufferInput.GetShaderNameHash()) ||
			core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
			"Failed to find buffer with the given shader name. This is is not an error, but a useful debugging helper");

		auto bufferTypeItr = shaderPlatObj->m_bufferMetadata.find(bufferInput.GetShaderNameHash());
		if (bufferTypeItr != shaderPlatObj->m_bufferMetadata.end())
		{
			const opengl::Buffer::BindTarget bindTarget = bufferTypeItr->second.m_bindTarget;

			const GLint bufferLoc = 
				bufferTypeItr->second.m_bufferLocations.at(bufferInput.GetView().m_bufferView.m_firstDestIdx);

			opengl::Buffer::Bind(*bufferInput.GetBuffer(), bindTarget, bufferInput.GetView(), bufferLoc);
		}
	}


	void Shader::SetTextureAndSampler(re::Shader const& shader, re::TextureAndSamplerInput const& texSamplerInput)
	{
		PlatObj const* platObj = shader.GetPlatformObject()->As<opengl::Shader::PlatObj const*>();
		SEAssert(platObj->m_isCreated == true, "Shader has not been created yet");

		// Bind the texture:
		auto const& textureBindingUnit = platObj->m_samplerUnits.find(texSamplerInput.m_shaderName);
		if (textureBindingUnit == platObj->m_samplerUnits.end())
		{
			
			SEAssert(core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
				std::format("Shader \"{}\" texture name \"{}\"is invalid, and strict shader binding is enabled",
					shader.GetName(), texSamplerInput.m_shaderName).c_str());
			return;
		}
		opengl::Texture::Bind(texSamplerInput.m_texture, textureBindingUnit->second, texSamplerInput.m_textureView);


		// Bind the sampler:
		auto const& samplerBindingUnit = platObj->m_samplerUnits.find(texSamplerInput.m_shaderName);
		if (samplerBindingUnit == platObj->m_samplerUnits.end())
		{
			SEAssert(core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
				std::format("Shader \"{}\" sampler name \"{}\"is invalid, and strict shader binding is enabled",
					shader.GetName(), texSamplerInput.m_shaderName).c_str());
			return;
		}
		opengl::Sampler::Bind(*texSamplerInput.m_sampler, samplerBindingUnit->second);
	}


	void Shader::SetImageTextureTargets(re::Shader const& shader, std::vector<re::RWTextureInput> const& rwTexInputs)
	{
		opengl::Shader::PlatObj const* platObj =
			shader.GetPlatformObject()->As<opengl::Shader::PlatObj const*>();

		for (uint32_t slot = 0; slot < rwTexInputs.size(); slot++)
		{
			re::RWTextureInput const& rwTexInput = rwTexInputs[slot];

			auto const& bindingUnit = platObj->m_samplerUnits.find(rwTexInput.m_shaderName);

			constexpr uint32_t k_accessMode = GL_READ_WRITE;
			opengl::Texture::BindAsImageTexture(rwTexInput.m_texture, bindingUnit->second, rwTexInput.m_textureView, k_accessMode);
		}
	}
}