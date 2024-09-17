// © 2024 Adam Badke. All rights reserved.
#include "ParseHelpers.h"
#include "ShaderPreprocessor_OpenGL.h"

#include "Core/Util/TextUtils.h"


namespace droid
{
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


	constexpr char const* k_globalPreamble =
		"#version 460 core\n"
		"#define SE_OPENGL\n"
		"\n"; // Note: MUST be terminated with "\n"


	std::string LoadIndividualShaderTextFile(
		std::vector<std::string> const& includeDirectories, std::string const& filenameAndExtension)
	{
		std::string shaderText;
		for (auto const& shaderDir : includeDirectories)
		{
			std::string filepath = shaderDir + filenameAndExtension;

			// Attempt to load the shader
			shaderText = util::LoadTextAsString(filepath);

			if (!shaderText.empty())
			{
				break;
			}
		}

		if (!shaderText.empty())
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


	std::string LoadShaderTextByExtension(
		std::vector<std::string> const& includeDirectories, std::string const& filename, re::Shader::ShaderType shaderType)
	{
		std::string const& filenameAndExtension = filename + ".glsl";

		return LoadIndividualShaderTextFile(includeDirectories, filenameAndExtension);
	}


	bool InsertIncludeText(
		std::vector<std::string> const& includeDirectories,
		std::string const& shaderText,
		std::vector<std::string>& shaderTextStrings,
		std::unordered_set<std::string>& seenIncludes)
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

							// Insert the first block
							const size_t blockLength = includeStartIdx - blockStartIdx;
							if (blockLength > 0) // 0 if we have several consecutive #defines
							{
								shaderTextStrings.emplace_back(shaderText.substr(blockStartIdx, blockLength));
							}

							// Extract the filename from the #include directive:
							const size_t includeFileNameStrLength = lastQuoteIndex - firstQuoteIndex;
							std::string const& includeFileName =
								shaderText.substr(firstQuoteIndex, includeFileNameStrLength);

							// Parse the include, but only if we've not seen it before:
							const bool newInclude = seenIncludes.emplace(includeFileName).second;
							if (newInclude)
							{
								std::string const& includeFile = 
									LoadIndividualShaderTextFile(includeDirectories, includeFileName);
								if (includeFile != "")
								{
									// Recursively parse the included file for nested #includes:
									const bool result = 
										InsertIncludeText(includeDirectories, includeFile, shaderTextStrings, seenIncludes);
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


	bool InsertIncludeText(
		std::vector<std::string> const& includeDirectories,
		std::string const& shaderText,
		std::vector<std::string>& shaderTextStrings)
	{
		std::unordered_set<std::string> seenIncludes;
		return InsertIncludeText(includeDirectories, shaderText, shaderTextStrings, seenIncludes);
	}


	droid::ErrorCode BuildShaderFile_GLSL(
		std::vector<std::string> const& includeDirectories,
		std::string const& extensionlessSrcFilename,
		uint64_t variantID,
		std::string const& entryPointName,
		re::Shader::ShaderType shaderType,
		std::vector<std::string> const& defines,
		std::string const& outputDir)
	{
		std::string const& outputFileName = std::format("{}.glsl",
			BuildExtensionlessShaderVariantName(extensionlessSrcFilename, variantID));

		std::string concatenatedDefines;
		for (auto const& define : defines)
		{
			concatenatedDefines = std::format("{} ", define);
		}

		std::string const& outputMsg = std::format("Building GLSL shader \"{}\"{}{}\n",
			outputFileName,
			concatenatedDefines.empty() ? "" : ", Defines = ",
			concatenatedDefines);
		std::cout << outputMsg.c_str();

		// Load the base shader file:
		std::string shaderText = LoadShaderTextByExtension(includeDirectories, extensionlessSrcFilename, shaderType);

		if (shaderText.empty())
		{
			std::cout << "Error: Failed to load GLSL shader text \"" << extensionlessSrcFilename.c_str() << "\"\n";
			return droid::ErrorCode::FileError;
		}

		// Add our preambles:
		std::vector<std::string> shaderTextStrings;
		shaderTextStrings.emplace_back(k_globalPreamble);
		shaderTextStrings.emplace_back(k_shaderPreambles[shaderType]);
		shaderTextStrings.emplace_back(std::format("#define {} main\n", entryPointName));

		for (auto const& define : defines)
		{
			shaderTextStrings.emplace_back(std::format("#define {}\n", define));
		}

		// Process the shader text, splitting it and inserting include files as they're encountered:
		const bool result = InsertIncludeText(includeDirectories, shaderText, shaderTextStrings);

		// Get the total reservation size we'll need:
		size_t requiredSize = 0;
		for (auto const& include : shaderTextStrings)
		{
			requiredSize += include.size();
		}
		shaderText.clear();
		shaderText.reserve(requiredSize);

		// Combine all of the include entries back into a single file:
		for (auto const& include : shaderTextStrings)
		{
			shaderText += include;
		}

		

		std::string const& combinedFilePath = std::format("{}{}", outputDir, outputFileName);

		std::ofstream outputStream;
		outputStream.open(combinedFilePath);
		if (!outputStream.is_open())
		{
			return droid::ErrorCode::FileError;
		}

		outputStream << shaderText.c_str();

		outputStream.close();

		return droid::ErrorCode::Success;
	}
}