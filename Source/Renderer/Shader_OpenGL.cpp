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
}


namespace opengl
{
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

		// Populate the uniform locations
		// Get the number of active uniforms found in the shader:
		int numUniforms = 0;
		glGetProgramiv(platParams->m_shaderReference, GL_ACTIVE_UNIFORMS, &numUniforms);

		// Get the max length of the active uniform names found in the shader:
		int maxUniformNameLength = 0;
		glGetProgramiv(platParams->m_shaderReference, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformNameLength);

		// Populate uniform metadata:
		int size = 0; // Size of the uniform variable; currently we just ignore this
		GLenum type; // Data type of the uniform
		std::vector<GLchar> name(maxUniformNameLength, '\0'); // Uniform name, as described in the shader text
		for (size_t uniformIdx = 0; uniformIdx < numUniforms; uniformIdx++)
		{
			// Get the size, type, and name of the uniform at the current index
			glGetActiveUniform(
				platParams->m_shaderReference,		// program
				static_cast<GLuint>(uniformIdx),	// index
				maxUniformNameLength,				// buffer size
				nullptr,							// length
				&size,								// size
				&type,								// type
				name.data());						// name

			if (UniformIsSamplerType(type))
			{
				const GLuint uniformLocation = glGetUniformLocation(platParams->m_shaderReference, name.data());

				// Get the texture unit binding value:
				GLint params = 0;
				glGetUniformiv(
					platParams->m_shaderReference,	// program
					uniformLocation,				// location
					&params);						// params

				// Populate the shader sampler unit map with unique entries:
				std::string nameStr(name.data());
				SEAssert(platParams->m_samplerUnits.find(nameStr) == platParams->m_samplerUnits.end(),
					"Sampler unit already found! Does the shader have a unique binding layout qualifier?");

				platParams->m_samplerUnits.emplace(std::move(nameStr), static_cast<int32_t>(params));
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


	void Shader::SetBuffer(re::Shader const& shader, re::BufferInput const& bufferInput)
	{
		opengl::Shader::PlatformParams const* shaderPlatformParams = 
			shader.GetPlatformParams()->As<opengl::Shader::PlatformParams const*>();

		SEAssert(shaderPlatformParams->m_isCreated == true, "Shader has not been created yet");
		
		GLint bindIndex = 0;
		const GLenum properties = GL_BUFFER_BINDING;

		re::Buffer::PlatformParams const* bufferPlatformParams = bufferInput.GetBuffer()->GetPlatformParams();
		switch (bufferInput.GetBuffer()->GetBufferParams().m_type)
		{
		case re::Buffer::Type::Constant: // Bind our single-element buffers as UBOs
		{
			// Find the buffer binding index via introspection
			const GLint uniformBlockIdx = glGetProgramResourceIndex(
				shaderPlatformParams->m_shaderReference,	// program
				GL_UNIFORM_BLOCK,							// programInterface
				bufferInput.GetShaderName().c_str());		// name

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
		case re::Buffer::Type::Structured: // Bind our array buffers as SSBOs, as they support dynamic indexing
		{
			// Find the buffer binding index via introspection
			const GLint ssboIdx = glGetProgramResourceIndex(
			shaderPlatformParams->m_shaderReference,	// program
			GL_SHADER_STORAGE_BLOCK,					// programInterface
			bufferInput.GetShaderName().c_str());		// name

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
		opengl::Buffer::Bind(*bufferInput.GetBuffer(), bindIndex);
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


	void Shader::SetImageTextureTargets(re::Shader const& shader, std::vector<re::RWTextureInput> const& rwTexInputs)
	{
		opengl::Shader::PlatformParams const* params =
			shader.GetPlatformParams()->As<opengl::Shader::PlatformParams const*>();

		for (uint32_t slot = 0; slot < rwTexInputs.size(); slot++)
		{
			re::RWTextureInput const& rwTexInput = rwTexInputs[slot];

			auto const& bindingUnit = params->m_samplerUnits.find(rwTexInput.m_shaderName);

			re::Texture const* texture = rwTexInput.m_texture;

			constexpr uint32_t k_accessMode = GL_READ_WRITE;
			opengl::Texture::BindAsImageTexture(*texture, bindingUnit->second, rwTexInput.m_textureView, k_accessMode);
		}
	}
}