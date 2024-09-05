// © 2024 Adam Badke. All rights reserved.
#include "EffectParsing.h"
#include "FileWriter.h"
#include "ParseDB.h"
#include "ShaderCompile_DX12.h"
#include "ShaderPreprocessor_OpenGL.h"
#include "TextStrings.h"

#include "Core/Definitions/ConfigKeys.h"

#include "Core/Util/FileIOUtils.h"
#include "Core/Util/HashKey.h"
#include "Core/Util/TextUtils.h"

#include "Renderer/EffectKeys.h"


namespace
{
	droid::ErrorCode ParseDrawStylesBlock(
		droid::ParseDB& parseDB, std::string const& effectName, auto const& drawStylesBlock)
	{
		// Parse the contents of a "DrawStyles" []:
		for (auto const& drawstyleEntry : drawStylesBlock)
		{
			if (!drawstyleEntry.contains(key_conditions) ||
				!drawstyleEntry.contains(key_technique))
			{
				return droid::ErrorCode::JSONError;
			}

			droid::ParseDB::DrawStyleTechnique drawStyleTechnique{};
			
			// "Conditions":
			for (auto const& condition : drawstyleEntry.at(key_conditions))
			{
				if (!condition.contains(key_rule) ||
					condition.at(key_rule).empty() ||
					!condition.contains(key_mode) ||
					condition.at(key_mode).empty())
				{
					return droid::ErrorCode::JSONError;
				}

				// "Rule":
				std::string rule = condition.at(key_rule);

				// "Mode":
				std::string mode = condition.at(key_mode);

				drawStyleTechnique.m_drawStyleConditions.emplace_back( rule, mode );
			}

			// "Technique":
			drawStyleTechnique.m_techniqueName = drawstyleEntry.at(key_technique).template get<std::string>();

			parseDB.AddEffectDrawStyleTechnique(effectName, std::move(drawStyleTechnique));
		}

		return droid::ErrorCode::Success;
	}


	void ParseVertexStreamsEntry(droid::ParseDB& parseDB, auto const& vertexStreamsEntry)
	{
		std::string const& streamsBlockName = vertexStreamsEntry.at(key_name);

		for (auto const& slotDesc : vertexStreamsEntry.at(key_slots))
		{
			std::string const& dataType = slotDesc.at(key_dataType).template get<std::string>();
			std::string const& name = slotDesc.at(key_name).template get<std::string>();
			std::string const& semantic = slotDesc.at(key_semantic).template get<std::string>();

			parseDB.AddVertexStreamSlot(streamsBlockName,
				droid::ParseDB::VertexStreamSlotDesc{
					.m_dataType = dataType,
					.m_name = name,
					.m_semantic = semantic,
				});
		}
	}


	droid::ErrorCode ParseTechniquesBlock(droid::ParseDB& parseDB, auto const& techniquesBlock)
	{
		for (auto const& techniqueEntry : techniquesBlock)
		{
			droid::ParseDB::TechniqueDesc techniqueDesc{};

			std::string const& techniqueName = techniqueEntry.at(key_name).template get<std::string>();

			// "ExcludedPlatforms":
			if (techniqueEntry.contains(key_excludedPlatforms))
			{
				// Record excluded platform names in lower case, later we'll check them against lowercase names
				for (auto const& excludedPlatform : techniqueEntry[key_excludedPlatforms])
				{
					techniqueDesc.m_excludedPlatforms.emplace(
						util::ToLower(excludedPlatform.template get<std::string>()));
				}
			}

			for (uint8_t i = 0; i < re::Shader::ShaderType_Count; ++i)
			{
				bool foundShaderName = false;
				if (techniqueEntry.contains(keys_shaderTypes[i]))
				{
					techniqueDesc.m_shaderNames[i] = techniqueEntry.at(keys_shaderTypes[i]).template get<std::string>();
					foundShaderName = true;
				}

				bool foundEntryPoint = false;
				if (techniqueEntry.contains(keys_entryPointNames[i]))
				{
					techniqueDesc.m_entryPointNames[i] = 
						techniqueEntry.at(keys_entryPointNames[i]).template get<std::string>();
					foundEntryPoint = true;
				}

				if (foundShaderName != foundEntryPoint)
				{
					std::cout << "Technique \"" << techniqueName.c_str() << "\" does not have corresponding shader "
						"name and entry point name entries\n";
					return droid::ErrorCode::JSONError;
				}
			}

			const droid::ErrorCode result = parseDB.AddTechnique(techniqueName, std::move(techniqueDesc));
			if (result != droid::ErrorCode::Success)
			{
				return result;
			}
		}
		
		return droid::ErrorCode::Success;
	}


	char const* DataTypeNameToGLSLDataTypeName(std::string const& dataTypeName)
	{
		util::HashKey dataTypeNameHash = util::HashKey::Create(dataTypeName);

		static const std::unordered_map<util::HashKey const, char const*> s_dataTypeNameToGLSLTypeName =
		{
			{util::HashKey("uint2"), "uvec2"},
			{util::HashKey("uint3"), "uvec3"},
			{util::HashKey("uint4"), "uvec4"},

			{util::HashKey("int2"), "ivec2"},
			{util::HashKey("int3"), "ivec3"},
			{util::HashKey("int4"), "ivec4"},

			{util::HashKey("float2"), "vec2"},
			{util::HashKey("float3"), "vec3"},
			{util::HashKey("float4"), "vec4"},

			{util::HashKey("float2x2"), "mat2"},
			{util::HashKey("float3x3"), "mat3"},
			{util::HashKey("float4x4"), "mat4"},
		};

		return s_dataTypeNameToGLSLTypeName.at(dataTypeNameHash);
	}
}


namespace droid
{
	ParseDB::ParseDB(ParseParams const& parseParams)
		: m_parseParams(parseParams)
	{		
	}


	droid::ErrorCode ParseDB::Parse()
	{
		std::string const& effectManifestPath = std::format("{}{}",
			m_parseParams.m_effectSourceDir,
			m_parseParams.m_effectManifestFileName);
			
		std::cout << "\nLoading effect manifest \"" << effectManifestPath.c_str() << "\"...\n";

		std::ifstream effectManifestInputStream(effectManifestPath);
		if (!effectManifestInputStream.is_open())
		{
			std::cout << "Error: Failed to open effect manifest input stream\n";
			return droid::ErrorCode::FileError;
		}
		std::cout << "Successfully opened effect manifest \"" << effectManifestPath.c_str() << "\"!\n\n";

		droid::ErrorCode result = droid::ErrorCode::Success;

		std::vector<std::string> effectNames;
		nlohmann::json effectManifestJSON;
		try
		{
			const nlohmann::json::parser_callback_t parserCallback = nullptr;
			effectManifestJSON = nlohmann::json::parse(
				effectManifestInputStream,
				parserCallback,
				m_parseParams.m_allowJSONExceptions,
				m_parseParams.m_ignoreJSONComments);

			// Build the list of effect names:
			for (auto const& effectManifestEntry : effectManifestJSON.at(key_effectsBlock))
			{
				std::string const& effectName = effectManifestEntry.template get<std::string>();
				effectNames.emplace_back(effectName);
			}

			std::cout << "Effect manifest successfully parsed!\n\n";
			effectManifestInputStream.close();
		}
		catch (nlohmann::json::exception parseException)
		{
			std::cout << std::format(
				"Failed to parse the Effect manifest file \"{}\"\n{}",
				effectManifestPath,
				parseException.what()).c_str();

			effectManifestInputStream.close();

			result = ErrorCode::JSONError;
		}

		// Parse the effect files listed in the manifest:
		for (auto const& effectName : effectNames)
		{
			result = ParseEffectFile(effectName, m_parseParams);
			if (result != droid::ErrorCode::Success)
			{
				break;
			}
		}

		return result;
	}


	droid::ErrorCode ParseDB::ParseEffectFile(std::string const& effectName, ParseParams const& parseParams)
	{
		std::cout << "Parsing Effect \"" << effectName.c_str() << "\":\n";
		
		std::string const& effectFileName = effectName + ".json";
		std::string const& effectFilePath = parseParams.m_effectSourceDir + effectFileName;

		std::ifstream effectInputStream(effectFilePath);
		if (!effectInputStream.is_open())
		{
			std::cout << "Error: Failed to open effect input stream" << effectFilePath << "\n";
			return droid::ErrorCode::FileError;
		}
		std::cout << "Successfully opened effect file \"" << effectFilePath.c_str() << "\"!\n\n";

		droid::ErrorCode result = droid::ErrorCode::Success;

		nlohmann::json effectJSON;
		try
		{
			const nlohmann::json::parser_callback_t parserCallback = nullptr;
			effectJSON = nlohmann::json::parse(
				effectInputStream,
				parserCallback,
				parseParams.m_allowJSONExceptions,
				parseParams.m_ignoreJSONComments);

			// "Effect":
			if (effectJSON.contains(key_effectBlock))
			{
				auto const& effectBlock = effectJSON.at(key_effectBlock);

				if (!effectBlock.contains(key_name))
				{
					result = droid::ErrorCode::JSONError;
				}
				if (result != droid::ErrorCode::Success)
				{
					return result;
				}

				std::string const& effectBlockName = effectBlock.at(key_name).template get<std::string>();
				if (effectBlockName != effectName)
				{
					result = droid::ErrorCode::JSONError;
				}
				if (result != droid::ErrorCode::Success)
				{
					return result;
				}

				// "DrawStyles":
				if (effectBlock.contains(key_drawStyles))
				{
					result = ParseDrawStylesBlock(*this, effectBlockName, effectBlock.at(key_drawStyles));
				}
				if (result != droid::ErrorCode::Success)
				{
					return result;
				}

				// "Techniques":
				if (effectBlock.contains(key_techniques))
				{
					result = ParseTechniquesBlock(*this, effectBlock.at(key_techniques));
					if (result != droid::ErrorCode::Success)
					{
						return result;
					}
				}
			}

			// "VertexStreams":
			if (effectJSON.contains(key_vertexStreams))
			{
				for (auto const& vertexStreamEntry : effectJSON.at(key_vertexStreams))
				{
					ParseVertexStreamsEntry(*this, vertexStreamEntry);
				}
			}

			std::cout << "Effect \"" << effectName.c_str() << "\" successfully parsed!\n\n";
		}
		catch (nlohmann::json::exception parseException)
		{
			std::cout << std::format(
				"Failed to parse the Effect file \"{}\"\n{}",
				effectName,
				parseException.what()).c_str();

			result = ErrorCode::JSONError;
		}

		effectInputStream.close();
		return result;
	}


	droid::ErrorCode ParseDB::GenerateCPPCode() const
	{
		std::cout << "Generating C++ code...\n";

		// Start by clearing out any previously generated code:
		droid::CleanDirectory(m_parseParams.m_cppCodeGenOutputDir.c_str());

		droid::ErrorCode result = droid::ErrorCode::Success;
			
		if (result == droid::ErrorCode::Success)
		{
			result = GenerateCPPCode_Drawstyle();
		}
		
		return result;
	}


	droid::ErrorCode ParseDB::GenerateShaderCode() const
	{
		std::cout << "Generating shader code...\n";

		// Start by clearing out any previously generated code:
		droid::CleanDirectory(m_parseParams.m_hlslCodeGenOutputDir.c_str());
		droid::CleanDirectory(m_parseParams.m_glslCodeGenOutputDir.c_str());

		droid::ErrorCode result = droid::ErrorCode::Success;

		if (result == droid::ErrorCode::Success)
		{
			result = GenerateShaderCode_VertexStreams();
		}

		return result;
	}


	droid::ErrorCode ParseDB::CompileShaders() const
	{
		droid::ErrorCode result = droid::ErrorCode::Success;

		// GLSL:
		{
			std::cout << "Building GLSL shader texts...\n";

			// Start by clearing out any previously generated code:
			droid::CleanDirectory(m_parseParams.m_glslShaderOutputDir.c_str());

			// Assemble a list of directories to search for shaders and #includes
			const std::vector<std::string> glslIncludeDirectories = {
				m_parseParams.m_glslShaderSourceDir,
				m_parseParams.m_glslCodeGenOutputDir,
				m_parseParams.m_commonShaderSourceDir,
				m_parseParams.m_dependenciesDir,				
			};

			for (auto const& technique : m_techniqueDescs)
			{
				for (uint8_t shaderTypeIdx = 0; shaderTypeIdx < re::Shader::ShaderType_Count; ++shaderTypeIdx)
				{
					if (technique.second.m_shaderNames[shaderTypeIdx].empty() || 
						technique.second.m_excludedPlatforms.contains("opengl"))
					{
						continue;
					}

					const std::vector<std::string> defines = {
						// TODO
					};
				
					result = BuildShaderFile_GLSL(
						glslIncludeDirectories,
						technique.second.m_shaderNames[shaderTypeIdx],
						technique.second.m_entryPointNames[shaderTypeIdx],
						static_cast<re::Shader::ShaderType>(shaderTypeIdx),
						defines,
						m_parseParams.m_glslShaderOutputDir);

					if (result != droid::ErrorCode::Success)
					{
						return result;
					}
				}				
			}
		}

		// HLSL:
		{
			std::cout << "Compiling HLSL shaders...\n";

			result = PrintHLSLCompilerVersion(m_parseParams.m_directXCompilerExePath);
			if (result != droid::ErrorCode::Success)
			{
				return result;
			}

			// Start by clearing out any previously generated code:
			droid::CleanDirectory(m_parseParams.m_hlslShaderOutputDir.c_str());

			// Populate the compile options based on the build configuration:
			HLSLCompileOptions compileOptions;
			switch (m_parseParams.m_buildConfiguration)
			{
			case util::BuildConfiguration::Debug:
			{
				compileOptions = HLSLCompileOptions {
					.m_disableOptimizations = true,
					.m_enableDebuggingInfo = true,
					.m_allResourcesBound = false, // Default
					.m_treatWarningsAsErrors = false, // Default
					.m_enable16BitTypes = false, // Default
					.m_targetProfile = "6_6", // Default
					.m_optimizationLevel = 0,
				};
			}
			break;
			case util::BuildConfiguration::DebugRelease:
			{
				compileOptions = HLSLCompileOptions{
					.m_disableOptimizations = true,
					.m_enableDebuggingInfo = true,
					.m_allResourcesBound = false, // Default
					.m_treatWarningsAsErrors = false, // Default
					.m_enable16BitTypes = false, // Default
					.m_targetProfile = "6_6", // Default
					.m_optimizationLevel = 0,
				};
			}
			break;
			case util::BuildConfiguration::Profile:
			{
				compileOptions = HLSLCompileOptions{
					.m_disableOptimizations = false,
					.m_enableDebuggingInfo = false,
					.m_allResourcesBound = false, // Default
					.m_treatWarningsAsErrors = false, // Default
					.m_enable16BitTypes = false, // Default
					.m_targetProfile = "6_6",
					.m_optimizationLevel = 3,
				};
			}
			break;
			case util::BuildConfiguration::Release:
			{
				compileOptions = HLSLCompileOptions{
					.m_disableOptimizations = false,
					.m_enableDebuggingInfo = false,
					.m_allResourcesBound = false, // Default
					.m_treatWarningsAsErrors = false, // Default
					.m_enable16BitTypes = false, // Default
					.m_targetProfile = "6_6",
					.m_optimizationLevel = 3,
				};
			}
			break;
			default: result = droid::ErrorCode::ConfigurationError;
			}
			if (result != droid::ErrorCode::Success)
			{
				return result;
			}

			const std::vector<std::string> hlslIncludeDirectories = {
				m_parseParams.m_hlslShaderSourceDir,
				m_parseParams.m_hlslCodeGenOutputDir,
				m_parseParams.m_commonShaderSourceDir,
				m_parseParams.m_dependenciesDir,
			};

			auto CloseProcess = [](PROCESS_INFORMATION const& processInfo) -> droid::ErrorCode
				{
					droid::ErrorCode result = droid::ErrorCode::Success;

					WaitForSingleObject(processInfo.hProcess, INFINITE); // Wait until the process is done

					DWORD exitCode = 0;
					GetExitCodeProcess(processInfo.hProcess, &exitCode);
					if (exitCode != 0)
					{
						std::cout << "HLSL compiler returned " << exitCode << "\n";
						result = droid::ErrorCode::ShaderError;
					}

					// Close process and thread handles
					CloseHandle(processInfo.hProcess);
					CloseHandle(processInfo.hThread);

					return result;
				};

			std::vector<PROCESS_INFORMATION> processInfos; // The HLSL shader is invoked as a seperate process

			for (auto const& technique : m_techniqueDescs)
			{
				for (uint8_t shaderTypeIdx = 0; shaderTypeIdx < re::Shader::ShaderType_Count; ++shaderTypeIdx)
				{
					if (technique.second.m_shaderNames[shaderTypeIdx].empty() ||
						technique.second.m_excludedPlatforms.contains("dx12"))
					{
						continue;
					}

					const std::vector<std::string> defines = {
						// TODO
					};

					PROCESS_INFORMATION& processInfo = processInfos.emplace_back();

					result = CompileShader_HLSL(
						m_parseParams.m_directXCompilerExePath,
						processInfo,
						compileOptions,
						hlslIncludeDirectories,
						technique.second.m_shaderNames[shaderTypeIdx],
						technique.second.m_entryPointNames[shaderTypeIdx],
						static_cast<re::Shader::ShaderType>(shaderTypeIdx),
						defines,
						m_parseParams.m_hlslShaderOutputDir);

					if (compileOptions.m_multithreadedCompilation == false)
					{
						result = CloseProcess(processInfo);
						processInfos.pop_back();
					}

					if (result != droid::ErrorCode::Success)
					{
						break;
					}
				}

				if (result != droid::ErrorCode::Success)
				{
					break;
				}
			}

			// Check our exit codes:
			for (auto const& processInfo : processInfos) // Will be empty if threading is disabled
			{
				const droid::ErrorCode processCloseResult = CloseProcess(processInfo);
				if (processCloseResult != droid::ErrorCode::Success && result == droid::ErrorCode::Success)
				{
					result = processCloseResult;
				}
			}
		}		

		return result;
	}


	droid::ErrorCode ParseDB::CopyEffects() const
	{
		// Ensure the output path is created
		if (!std::filesystem::exists(m_parseParams.m_runtimeEffectsDir))
		{
			std::filesystem::create_directories(m_parseParams.m_runtimeEffectsDir);
		}

		// Copy each .json file in the source directory:
		std::vector<std::string> const& effectFiles =
			util::GetDirectoryFilenameContents(m_parseParams.m_effectSourceDir.c_str(), ".json");

		for (auto const& effectSrcFilePath : effectFiles)
		{
			std::ifstream srcStream(effectSrcFilePath);
		
			std::string const& filename = std::filesystem::path(effectSrcFilePath).filename().string();
			std::ofstream dstStream(m_parseParams.m_runtimeEffectsDir + filename);

			if (!srcStream.is_open())
			{
				std::cout << "Failed to open source Effect file\n";
				return droid::ErrorCode::FileError;
			}
			if (!dstStream.is_open())
			{
				std::cout << "Failed to open destination Effects file\n";
				return droid::ErrorCode::FileError;
			}

			dstStream << srcStream.rdbuf();
		}

		return droid::ErrorCode::Success;
	}


	droid::ErrorCode ParseDB::GenerateCPPCode_Drawstyle() const
	{
		FileWriter filewriter(m_parseParams.m_cppCodeGenOutputDir, m_drawstyleHeaderFilename);

		droid::ErrorCode result = filewriter.GetStatus();
		if (result != droid::ErrorCode::Success)
		{
			return filewriter.GetStatus();
		}

		filewriter.OpenNamespace("effect::drawstyle");

		// Bitmasks:
		{
			filewriter.WriteLine("using Bitmask = uint64_t;");
			filewriter.EmptyLine();

			filewriter.WriteLine("constexpr Bitmask DefaultTechnique = 0;");

			uint8_t bitIdx = 0;
			for (auto const& drawstyle : m_drawStyleRuleToModes)
			{
				std::string const& rule = drawstyle.first;
				for (auto const& mode : drawstyle.second)
				{
					filewriter.WriteLine(std::format("constexpr Bitmask {}_{} = 1llu << {};",
						rule, mode, bitIdx++));
				}
			}
			if (bitIdx >= 64)
			{
				std::cout << "Error: " << " is too many drawstyle rules to fit in a 64-bit bitmask\n";
				result = droid::ErrorCode::GenerationError;
			}
		}

		// Static functions:
		{
			filewriter.EmptyLine();
			filewriter.WriteLine("using ModeToBitmask = std::unordered_map<util::HashKey const, effect::drawstyle::Bitmask>;");
			filewriter.WriteLine("using DrawStyleRuleToModes = std::unordered_map<util::HashKey const, ModeToBitmask>;");

			filewriter.EmptyLine();

			// GetDrawStyleRuleToModesMap():
			filewriter.WriteLine("static DrawStyleRuleToModes const& GetDrawStyleRuleToModesMap()");
			filewriter.OpenBrace();

			filewriter.WriteLine("static const DrawStyleRuleToModes s_drawstyleBitmaskMappings({");
			filewriter.Indent();

			for (auto const& drawstyle : m_drawStyleRuleToModes)
			{
				std::string const& rule = drawstyle.first;

				filewriter.WriteLine("{");
				filewriter.Indent();

				filewriter.WriteLine(std::format("util::HashKey(\"{}\"),", rule));
				filewriter.OpenBrace();
				for (auto const& mode : drawstyle.second)
				{
					filewriter.WriteLine(std::format("{{util::HashKey(\"{}\"), effect::drawstyle::{}_{}}},",
						mode,
						rule,
						mode));
				}
				filewriter.CloseBrace();

				filewriter.Unindent();
				filewriter.WriteLine("},");
			}

			filewriter.Unindent();
			filewriter.WriteLine("});");
		
			filewriter.WriteLine("return s_drawstyleBitmaskMappings;");			

			filewriter.CloseBrace();
		}

		filewriter.CloseNamespace();

		return result;
	}


	droid::ErrorCode ParseDB::GenerateShaderCode_VertexStreams() const
	{
		droid::ErrorCode result = droid::ErrorCode::Success;
		
		for (auto const& vertexStreamDesc : m_vertexStreamDescs)
		{
			std::string const& hlslFilename = std::format("{}{}.hlsli", 
				m_vertexStreamsFilenamePrefex,
				vertexStreamDesc.first);

			std::string const& glslFilename = std::format("{}{}.glsl",
				m_vertexStreamsFilenamePrefex,
				vertexStreamDesc.first);

			FileWriter hlslWriter(m_parseParams.m_hlslCodeGenOutputDir, hlslFilename);
			FileWriter glslWriter(m_parseParams.m_glslCodeGenOutputDir, glslFilename);

			std::string vertexStreamNameUpperCase = vertexStreamDesc.first;
			std::transform(
				vertexStreamNameUpperCase.begin(),
				vertexStreamNameUpperCase.end(),
				vertexStreamNameUpperCase.begin(), ::toupper);

			std::string const& hlslIncludeGuard = std::format("{}_VERTEXSTREAM_HLSL", vertexStreamNameUpperCase);
			std::string const& glslIncludeGuard = std::format("{}_VERTEXSTREAM_GLSL", vertexStreamNameUpperCase);

			hlslWriter.WriteLine(std::format("#ifndef {}", hlslIncludeGuard));
			hlslWriter.WriteLine(std::format("#define {}", hlslIncludeGuard));

			glslWriter.WriteLine(std::format("#ifndef {}", glslIncludeGuard));
			glslWriter.WriteLine(std::format("#define {}", glslIncludeGuard));

			hlslWriter.EmptyLine();
			glslWriter.EmptyLine();

			hlslWriter.WriteLine("struct VertexIn");
			hlslWriter.OpenStructBrace();

			uint8_t slotIdx = 0;
			for (auto const& slotDesc : vertexStreamDesc.second)
			{
				// HLSL:
				{
					hlslWriter.WriteLine(std::format("{} {} : {};",
						slotDesc.m_dataType,
						slotDesc.m_name,
						slotDesc.m_semantic));
				}

				//GLSL:
				{
					glslWriter.WriteLine(std::format("layout(location = {}) in {} {};",
						slotIdx,
						DataTypeNameToGLSLDataTypeName(slotDesc.m_dataType),
						slotDesc.m_name));
				}

				++slotIdx;
			}

			// TODO: Only add this when instancing is explicitely specified in the Effect definition
			hlslWriter.EmptyLine();
			hlslWriter.WriteLine("uint InstanceID : SV_InstanceID;");
			
			hlslWriter.CloseStructBrace();
			glslWriter.EmptyLine();

			hlslWriter.WriteLine(std::format("#endif // {}", hlslIncludeGuard));
			glslWriter.WriteLine(std::format("#endif // {}", glslIncludeGuard));
		}

		return result;
	}
}