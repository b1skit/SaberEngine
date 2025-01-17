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
#include "TextureTarget.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/PerformanceTimer.h"
#include "Core/ThreadPool.h"

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

		GL_COMPUTE_SHADER
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

		opengl::Shader::PlatformParams* platParams = shader.GetPlatformParams()->As<opengl::Shader::PlatformParams*>();

		// Populate the uniform locations
		// Get the number of active uniforms found in the shader:
		GLint numUniforms = 0;
		glGetProgramiv(platParams->m_shaderReference, GL_ACTIVE_UNIFORMS, &numUniforms);

		// Get the max length of the active uniform names found in the shader:
		GLint maxUniformNameLength = 0;
		glGetProgramiv(platParams->m_shaderReference, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformNameLength);

		// Sampler uniforms:
		GLint uniformSize = 0; // Size of the uniform variable; currently we just ignore this
		GLenum uniformType; // Data type of the uniform
		std::vector<GLchar> samplerName(maxUniformNameLength, '\0'); // Uniform name, as described in the shader text
		for (GLint uniformIdx = 0; uniformIdx < numUniforms; uniformIdx++)
		{
			// Get the size, type, and name of the uniform at the current index
			glGetActiveUniform(
				platParams->m_shaderReference,		// program
				static_cast<GLuint>(uniformIdx),	// index
				maxUniformNameLength,				// buffer size
				nullptr,							// length
				&uniformSize,						// size
				&uniformType,						// type
				samplerName.data());				// name

			if (UniformIsSamplerType(uniformType))
			{
				const GLuint uniformLocation = glGetUniformLocation(platParams->m_shaderReference, samplerName.data());

				// Get the texture unit binding value:
				GLint bindIdx = 0;
				glGetUniformiv(
					platParams->m_shaderReference,	// program
					uniformLocation,				// location
					&bindIdx);						// params

				// Populate the shader sampler unit map with unique entries:
				std::string nameStr(samplerName.data());
				SEAssert(platParams->m_samplerUnits.find(nameStr) == platParams->m_samplerUnits.end(),
					"Sampler unit already found! Does the shader have a unique binding layout qualifier?");

				platParams->m_samplerUnits.emplace(std::move(nameStr), bindIdx);

#if defined(LOG_SHADER_NAMES)
				LOG("Shader \"%s\": Found sampler uniform %s = %d",
					shader.GetName().c_str(), samplerName.data(), bindIdx);
#endif
			}
		}

		// Vertex attributes:
		GLint numAttributes = 0;
		glGetProgramiv(platParams->m_shaderReference, GL_ACTIVE_ATTRIBUTES, &numAttributes);

		GLint maxAttributeNameLength = 0;
		glGetProgramiv(platParams->m_shaderReference, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxAttributeNameLength);

		GLint attributeSize = 0; // Size of the uniform variable; currently we just ignore this
		GLenum attributeType; // Data type of the uniform
		std::vector<GLchar> attributeName(maxAttributeNameLength, '\0'); // Attribute name, as described in the shader text

		for (GLint attributeIdx = 0; attributeIdx < numAttributes; attributeIdx++)
		{
			glGetActiveAttrib(
				platParams->m_shaderReference,		// program
				static_cast<GLuint>(attributeIdx),	// index
				maxAttributeNameLength,				// buffer size
				nullptr,							// length
				&attributeSize,						// size
				&attributeType,						// type
				attributeName.data());				// name

			const GLint attributeLocation = glGetAttribLocation(platParams->m_shaderReference, attributeName.data());

			if (attributeLocation >= 0) // -1 for gl_InstanceID, gl_VertexID etc
			{
				platParams->m_vertexAttributeLocations.emplace(attributeName.data(), attributeLocation);

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
			platParams->m_shaderReference, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &numActiveUniformBlocks);

		for (GLint uboIdx = 0; uboIdx < numActiveUniformBlocks; ++uboIdx)
		{
			glGetProgramResourceName(
				platParams->m_shaderReference,					// program
				GL_UNIFORM_BLOCK,								// programInterface
				static_cast<GLuint>(uboIdx),			// index
				static_cast<GLsizei>(k_maxResourceNameLength),	// bufSize
				nullptr,										// length (optional)
				resourceName.data());							// name

			GLint uboBindIdx = 0;

			glGetProgramResourceiv(
				platParams->m_shaderReference,
				GL_UNIFORM_BLOCK,
				uboIdx,
				1,
				&k_bufferProperty,
				1,
				NULL,
				&uboBindIdx);
			SEAssert(uboBindIdx >= 0, "Invalid buffer bind index returned");

			constexpr opengl::Buffer::BindTarget k_bindTarget = opengl::Buffer::BindTarget::UBO;

			platParams->AddBufferMetadata(resourceName.data(), k_bindTarget, uboBindIdx);

#if defined(LOG_SHADER_NAMES)
			LOG("Shader \"%s\": Found UBO %s = %d", shader.GetName().c_str(), resourceName.data(), uboBindIdx);
#endif
		}

		// SSBOs:
		GLint numActiveShaderStorageBlocks = 0;
		glGetProgramInterfaceiv(
			platParams->m_shaderReference, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &numActiveShaderStorageBlocks);

		for (GLint ssboIdx = 0; ssboIdx < numActiveShaderStorageBlocks; ++ssboIdx)
		{
			glGetProgramResourceName(
				platParams->m_shaderReference,					// program
				GL_SHADER_STORAGE_BLOCK,						// programInterface
				static_cast<GLuint>(ssboIdx),					// index
				static_cast<GLsizei>(k_maxResourceNameLength),	// bufSize
				nullptr,										// length (optional)
				resourceName.data());							// name

			GLint storageBlockBindIdx = 0;

			glGetProgramResourceiv(
				platParams->m_shaderReference,
				GL_SHADER_STORAGE_BLOCK,
				ssboIdx,
				1,
				&k_bufferProperty,
				1,
				NULL,
				&storageBlockBindIdx);

			constexpr opengl::Buffer::BindTarget k_bindTarget = opengl::Buffer::BindTarget::SSBO;

			platParams->AddBufferMetadata(resourceName.data(), k_bindTarget, storageBlockBindIdx);

#if defined(LOG_SHADER_NAMES)
			LOG("Shader \"%s\": Found SSBO %s = %d", shader.GetName().c_str(), resourceName.data(), storageBlockBindIdx);
#endif
		}
	}
}


namespace opengl
{
	void opengl::Shader::PlatformParams::AddBufferMetadata(
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
				opengl::Shader::PlatformParams::BufferMetadata{
				.m_bindTarget = bindTarget,
				.m_bufferLocations = std::move(bufLocations),
				});
		}
	}


	void Shader::Create(re::Shader& shader)
	{
		util::PerformanceTimer timer;
		timer.Start();

		opengl::Shader::PlatformParams* platParams = shader.GetPlatformParams()->As<opengl::Shader::PlatformParams*>();

		SEAssert(!platParams->m_isCreated, "Shader has already been created");
		platParams->m_isCreated = true;

		std::string const& shaderFileName = shader.GetName();
		LOG("Creating shader: \"%s\"", shaderFileName.c_str());

		// Load the individual shader text files:
		std::vector<std::future<void>> const& loadShaderTextsTaskFutures = 
			LoadShaderTexts(shader.m_extensionlessSourceFilenames, platParams->m_shaderTexts);

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
			if (!platParams->m_shaderTexts[i].empty())
			{
				foundShaderTypeFlags[i] = k_shaderTypeFlags[i]; // Mark the shader as seen
				shaderFiles[i] = std::move(platParams->m_shaderTexts[i]); // Move the shader texts, they're no longer needed
				shaderFileNames[i] = shaderFileName + ".glsl";
			}
		}
		SEAssert(foundShaderTypeFlags[re::Shader::Vertex] != 0 || foundShaderTypeFlags[re::Shader::Compute] != 0,
			"No shader found. Must have a vertex or compute shader at minimum");

		SEAssert(foundShaderTypeFlags[re::Shader::Mesh] == 0 && foundShaderTypeFlags[re::Shader::Amplification] == 0,
			"Mesh and amplification shaders are currently only supported via an NVidia extension (and not on AMD). For"
			"now, we don't support them.");

		// Create an empty shader program object:
		platParams->m_shaderReference = glCreateProgram();

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

			glAttachShader(platParams->m_shaderReference, shaderObject); // Attach our shaders to the shader program

			// Delete the shader stage now that we've attached it
			glDeleteShader(shaderObject);
		}

		// Link our program object:
		glLinkProgram(platParams->m_shaderReference);
		AssertShaderIsValid(shader.GetName(), platParams->m_shaderReference, GL_LINK_STATUS, true/*= isProgram*/);

		// Validate our program objects can execute with our current OpenGL state:
		glValidateProgram(platParams->m_shaderReference);
		AssertShaderIsValid(shader.GetName(), platParams->m_shaderReference, GL_VALIDATE_STATUS, true/*= isProgram*/);

		BuildShaderReflection(shader);

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
		opengl::Shader::PlatformParams const* params = 
			shader.GetPlatformParams()->As<opengl::Shader::PlatformParams const*>();
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
			SEAssertF("TODO: Re-implement this with support for core::InvPtr<re::Texture>");

			//auto const& bindingUnit = params->m_samplerUnits.find(uniformName);
			//if (bindingUnit == params->m_samplerUnits.end())
			//{
			//	SEAssert(core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
			//		std::format("Shader \"{}\" texture name \"{}\"is invalid, and strict shader binding is enabled", 
			//			shader.GetName(), uniformName).c_str());
			//	return;
			//}

			//opengl::Texture::Bind(*static_cast<re::Texture const*>(value), bindingUnit->second);
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


	void Shader::SetBuffer(re::Shader const& shader, re::BufferInput const& bufferInput)
	{
		opengl::Shader::PlatformParams const* shaderPlatParams = 
			shader.GetPlatformParams()->As<opengl::Shader::PlatformParams const*>();

		SEAssert(shaderPlatParams->m_isCreated == true, "Shader has not been created yet");
		
		re::Buffer::PlatformParams const* bufferPlatformParams = bufferInput.GetBuffer()->GetPlatformParams();

		SEAssert(shaderPlatParams->m_bufferMetadata.contains(bufferInput.GetShaderNameHash()) ||
			core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
			"Failed to find buffer with the given shader name. This is is not an error, but a useful debugging helper");

		auto bufferTypeItr = shaderPlatParams->m_bufferMetadata.find(bufferInput.GetShaderNameHash());
		if (bufferTypeItr != shaderPlatParams->m_bufferMetadata.end())
		{
			const opengl::Buffer::BindTarget bindTarget = bufferTypeItr->second.m_bindTarget;

			const GLint bufferLoc = 
				bufferTypeItr->second.m_bufferLocations.at(bufferInput.GetView().m_buffer.m_firstDestIdx);

			opengl::Buffer::Bind(*bufferInput.GetBuffer(), bindTarget, bufferInput.GetView(), bufferLoc);
		}
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
		opengl::Texture::Bind(texSamplerInput.m_texture, textureBindingUnit->second, texSamplerInput.m_textureView);


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


	void Shader::SetImageTextureTargets(re::Shader const& shader, std::vector<re::RWTextureInput> const& rwTexInputs)
	{
		opengl::Shader::PlatformParams const* params =
			shader.GetPlatformParams()->As<opengl::Shader::PlatformParams const*>();

		for (uint32_t slot = 0; slot < rwTexInputs.size(); slot++)
		{
			re::RWTextureInput const& rwTexInput = rwTexInputs[slot];

			auto const& bindingUnit = params->m_samplerUnits.find(rwTexInput.m_shaderName);

			constexpr uint32_t k_accessMode = GL_READ_WRITE;
			opengl::Texture::BindAsImageTexture(rwTexInput.m_texture, bindingUnit->second, rwTexInput.m_textureView, k_accessMode);
		}
	}
}