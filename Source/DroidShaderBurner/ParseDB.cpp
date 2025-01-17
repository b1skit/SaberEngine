// © 2024 Adam Badke. All rights reserved.
#include "EffectParsing.h"
#include "FileWriter.h"
#include "ParseDB.h"
#include "ParseHelpers.h"
#include "ShaderCompile_DX12.h"
#include "ShaderPreprocessor_OpenGL.h"
#include "TextStrings.h"

#include "Core/Definitions/ConfigKeys.h"

#include "Core/Util/FileIOUtils.h"
#include "Core/Util/CHashKey.h"
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


	droid::ErrorCode ParseVertexStreamsEntry(droid::ParseDB& parseDB, auto const& vertexStreamsEntry)
	{
		uint8_t numStreams = 0;

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
			numStreams++;

			if (numStreams > gr::VertexStream::k_maxVertexStreams)
			{
				std::cout << "Error: Trying to add too many vertex streams\n";
				return droid::ErrorCode::JSONError;
			}
		}

		return droid::ErrorCode::Success;
	}


	droid::ErrorCode ParseTechniquesBlock(
		droid::ParseDB& parseDB, std::string const& owningEffectName, auto const& techniquesBlock)
	{
		for (auto const& techniqueEntry : techniquesBlock)
		{
			droid::TechniqueDesc newTechnique = techniqueEntry;

			// "Parent": Handle inheritance:
			if (techniqueEntry.contains(key_parent))
			{
				std::string const& parentName = techniqueEntry.at(key_parent).template get<std::string>();

				if (!parseDB.HasTechnique(owningEffectName, parentName))
				{
					std::cout << "Error: Parent \"" << parentName.c_str() << "\" not found in Effect \"" << 
						owningEffectName.c_str() << "\"\n";
					return droid::ErrorCode::JSONError;
				}

				droid::TechniqueDesc const& parent = parseDB.GetTechnique(owningEffectName, parentName);

				newTechnique.InheritFrom(parent);
			}

			const droid::ErrorCode result = parseDB.AddTechnique(owningEffectName, std::move(newTechnique));
			if (result != droid::ErrorCode::Success)
			{
				return result;
			}
		}
		
		return droid::ErrorCode::Success;
	}


	char const* DataTypeNameToGLSLDataTypeName(std::string const& dataTypeName)
	{
		util::CHashKey dataTypeNameHash = util::CHashKey::Create(dataTypeName);

		static const std::unordered_map<util::CHashKey, char const*> s_dataTypeNameToGLSLTypeName =
		{
			{util::CHashKey("uint2"), "uvec2"},
			{util::CHashKey("uint3"), "uvec3"},
			{util::CHashKey("uint4"), "uvec4"},

			{util::CHashKey("int2"), "ivec2"},
			{util::CHashKey("int3"), "ivec3"},
			{util::CHashKey("int4"), "ivec4"},

			{util::CHashKey("float2"), "vec2"},
			{util::CHashKey("float3"), "vec3"},
			{util::CHashKey("float4"), "vec4"},

			{util::CHashKey("float2x2"), "mat2"},
			{util::CHashKey("float3x3"), "mat3"},
			{util::CHashKey("float4x4"), "mat4"},
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

			// Finally, write the runtime version of the manifest file out:
			result = WriteRuntimeEffectFile(effectManifestJSON, m_parseParams.m_effectManifestFileName);
			if (result != droid::ErrorCode::Success)
			{
				return result;
			}
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
					result = ParseTechniquesBlock(*this, effectBlockName, effectBlock.at(key_techniques));
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
					result = ParseVertexStreamsEntry(*this, vertexStreamEntry);
					if (result != droid::ErrorCode::Success)
					{
						return result;
					}
				}
			}

			std::cout << "Effect \"" << effectName.c_str() << "\" successfully parsed!\n\n";

			// Post-process Effects for runtime:
			PostProcessEffectTechniques(effectJSON, effectName);

			// Finally, write the runtime version of the file out:
			result = WriteRuntimeEffectFile(effectJSON, effectFileName);
			if (result != droid::ErrorCode::Success)
			{
				return result;
			}
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


	droid::ErrorCode ParseDB::PostProcessEffectTechniques(nlohmann::json& effectJSON, std::string const& effectName)
	{
		if (!m_effectTechniqueDescs.contains(effectName) || 
			!effectJSON.contains(key_effectBlock))
		{
			return droid::ErrorCode::Success; // Nothing to do
		}

		// "Effect":
		auto& effectBlock = effectJSON.at(key_effectBlock);

		// "Techniques":
		if (effectBlock.contains(key_techniques))
		{
			// Remove the existing Technique entries:
			effectBlock.at(key_techniques).clear();

			// Rebuild them as a vector:
			std::vector<TechniqueDesc> resolvedTechniques;
			resolvedTechniques.reserve(m_effectTechniqueDescs.at(effectName).size());

			for (auto const& entry : m_effectTechniqueDescs.at(effectName))
			{
				std::string const& techniqueName = entry.first;
				TechniqueDesc const& techniqueDesc = entry.second;

				resolvedTechniques.emplace_back(techniqueDesc);
			}

			// Our ParseTypes.h::to_json function will automatically resolve Techniques for runtime use
			effectBlock[key_techniques] = resolvedTechniques;
		}

		return droid::ErrorCode::Success;
	}


	droid::ErrorCode ParseDB::WriteRuntimeEffectFile(auto const& effectJSON, std::string const& effectFileName)
	{
		if (!std::filesystem::exists(m_parseParams.m_runtimeEffectsDir))
		{
			std::filesystem::create_directory(m_parseParams.m_runtimeEffectsDir);
		}

		std::string const& runtimeEffectFilePath = m_parseParams.m_runtimeEffectsDir + effectFileName;
		std::ofstream runtimeEffectOut(runtimeEffectFilePath);
		if (!runtimeEffectOut.is_open())
		{
			std::cout << "Error: Failed to open runtime Effect directory\n";
			return droid::ErrorCode::FileError;
		}

		runtimeEffectOut << std::setw(4) << effectJSON << std::endl;

		runtimeEffectOut.close();

		return droid::ErrorCode::Success;
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

			std::map<std::string, std::set<uint64_t>> seenShaderNamesAndVariants;

			for (auto const& effect : m_effectTechniqueDescs)
			{
				for (auto const& technique : effect.second)
				{
					for (uint8_t shaderTypeIdx = 0; shaderTypeIdx < re::Shader::ShaderType_Count; ++shaderTypeIdx)
					{
						if (technique.second._Shader[shaderTypeIdx].empty() ||
							technique.second.ExcludedPlatforms.contains("opengl"))
						{
							continue;
						}

						if (!seenShaderNamesAndVariants.contains(technique.second._Shader[shaderTypeIdx]))
						{
							seenShaderNamesAndVariants.emplace(technique.second._Shader[shaderTypeIdx], std::set<uint64_t>());
						}

						std::set<uint64_t>& variants = seenShaderNamesAndVariants.at(technique.second._Shader[shaderTypeIdx]);

						if (!variants.contains(technique.second.m_shaderVariantIDs[shaderTypeIdx]))
						{
							result = BuildShaderFile_GLSL(
								glslIncludeDirectories,
								technique.second._Shader[shaderTypeIdx],
								technique.second.m_shaderVariantIDs[shaderTypeIdx],
								technique.second._ShaderEntryPoint[shaderTypeIdx],
								static_cast<re::Shader::ShaderType>(shaderTypeIdx),
								technique.second._Defines[shaderTypeIdx],
								m_parseParams.m_glslShaderOutputDir);

							if (result != droid::ErrorCode::Success)
							{
								return result;
							}

							variants.emplace(technique.second.m_shaderVariantIDs[shaderTypeIdx]);
						}
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

			std::map<std::string, std::set<uint64_t>> seenShaderNamesAndVariants;

			for (auto const& effect : m_effectTechniqueDescs)
			{
				for (auto const& technique : effect.second)
				{
					for (uint8_t shaderTypeIdx = 0; shaderTypeIdx < re::Shader::ShaderType_Count; ++shaderTypeIdx)
					{
						if (technique.second._Shader[shaderTypeIdx].empty() ||
							technique.second.ExcludedPlatforms.contains("dx12"))
						{
							continue;
						}

						if (!seenShaderNamesAndVariants.contains(technique.second._Shader[shaderTypeIdx]))
						{
							seenShaderNamesAndVariants.emplace(technique.second._Shader[shaderTypeIdx], std::set<uint64_t>());
						}

						std::set<uint64_t>& variants = seenShaderNamesAndVariants.at(technique.second._Shader[shaderTypeIdx]);

						if (!variants.contains(technique.second.m_shaderVariantIDs[shaderTypeIdx]))
						{
							PROCESS_INFORMATION& processInfo = processInfos.emplace_back();

							result = CompileShader_HLSL(
								m_parseParams.m_directXCompilerExePath,
								processInfo,
								compileOptions,
								hlslIncludeDirectories,
								technique.second._Shader[shaderTypeIdx],
								technique.second.m_shaderVariantIDs[shaderTypeIdx],
								technique.second._ShaderEntryPoint[shaderTypeIdx],
								static_cast<re::Shader::ShaderType>(shaderTypeIdx),
								technique.second._Defines[shaderTypeIdx],
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

							variants.emplace(technique.second.m_shaderVariantIDs[shaderTypeIdx]);
						}
					}

					if (result != droid::ErrorCode::Success)
					{
						break;
					}
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


	droid::ErrorCode ParseDB::GenerateCPPCode_Drawstyle() const
	{
		FileWriter filewriter(m_parseParams.m_cppCodeGenOutputDir, m_drawstyleHeaderFilename);

		droid::ErrorCode result = filewriter.GetStatus();
		if (result != droid::ErrorCode::Success)
		{
			return filewriter.GetStatus();
		}

		filewriter.WriteLine("#pragma once");
		filewriter.EmptyLine();

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
			filewriter.WriteLine("using ModeToBitmask = std::unordered_map<util::CHashKey, effect::drawstyle::Bitmask>;");
			filewriter.WriteLine("using DrawStyleRuleToModes = std::unordered_map<util::CHashKey, ModeToBitmask>;");

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

				filewriter.WriteLine(std::format("util::CHashKey(\"{}\"),", rule));
				filewriter.OpenBrace();
				for (auto const& mode : drawstyle.second)
				{
					filewriter.WriteLine(std::format("{{util::CHashKey(\"{}\"), effect::drawstyle::{}_{}}},",
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

			std::string const& glslFilename = std::format("{}{}.glsli",
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

			// TODO: Only add these when explicitely requested in the Effect definition
			hlslWriter.EmptyLine();
			hlslWriter.WriteLine("uint InstanceID : SV_InstanceID;");
			hlslWriter.WriteLine("uint VertexID : SV_VertexID;");
			
			hlslWriter.CloseStructBrace();
			glslWriter.EmptyLine();

			hlslWriter.WriteLine(std::format("#endif // {}", hlslIncludeGuard));
			glslWriter.WriteLine(std::format("#endif // {}", glslIncludeGuard));
		}

		return result;
	}
}