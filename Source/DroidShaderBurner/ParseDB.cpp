// © 2024 Adam Badke. All rights reserved.
#include "EffectParsing.h"
#include "FileWriter.h"
#include "ParseDB.h"
#include "ShaderPreprocessor_OpenGL.h"
#include "TextStrings.h"

#include "Core/Definitions/ConfigKeys.h"
#include "Core/Definitions/EffectKeys.h"

#include "Core/Util/HashKey.h"


namespace
{
	void ParseDrawStyleConditionEntry(droid::ParseDB& parseDB, auto const& drawStylesEntry)
	{
		// Parse the contents of a "DrawStyles" []:
		for (auto const& drawstyleBlock : drawStylesEntry)
		{
			// "Conditions":"
			if (drawstyleBlock.contains(key_conditions))
			{
				for (auto const& condition : drawstyleBlock.at(key_conditions))
				{
					// "Rule":
					std::string rule;
					if (condition.contains(key_rule))
					{
						rule = condition.at(key_rule);
					}

					// "Mode":
					std::string mode;
					if (condition.contains(key_mode))
					{
						mode = condition.at(key_mode);
					}

					if (!rule.empty() && !mode.empty())
					{
						parseDB.AddDrawstyle(rule, mode);
					}
				}
			}
		}
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
				for (auto const& excludedPlatform : techniqueEntry[key_excludedPlatforms])
				{
					techniqueDesc.m_excludedPlatforms.emplace(excludedPlatform.template get<std::string>());
				}
			}

			for (uint8_t i = 0; i < re::Shader::ShaderType_Count; ++i)
			{
				if (techniqueEntry.contains(keys_shaderTypes[i]))
				{
					techniqueDesc.m_shaderNames[i] = techniqueEntry.at(keys_shaderTypes[i]).template get<std::string>();
				}
			}

			const droid::ErrorCode result = parseDB.AddTechnique(techniqueName, std::move(techniqueDesc));
			if (result != droid::ErrorCode::Success)
			{
				return result;
			}
		}
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
			m_parseParams.m_effectsDir,
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
		std::string const& effectFilePath = parseParams.m_effectsDir + effectFileName;

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

				// "DrawStyles":
				if (effectBlock.contains(key_drawStyles))
				{
					ParseDrawStyleConditionEntry(*this, effectBlock.at(key_drawStyles));
				}
			}

			// "Techniques":
			if (effectJSON.contains(key_techniques))
			{
				result = ParseTechniquesBlock(*this, effectJSON.at(key_techniques));
				if (result != droid::ErrorCode::Success)
				{
					return result;
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

		// Start by clearing out any previously generated code:
		droid::CleanDirectory(m_parseParams.m_glslShaderOutputDir.c_str());

		// GLSL:
		{
			std::cout << "Building GLSL shaders...\n";

			// Assemble a list of directories to search for shaders and #includes
			const std::vector<std::string> glslShaderDirs = {
				m_parseParams.m_glslShaderSourceDir,
				m_parseParams.m_glslCodeGenOutputDir,
				m_parseParams.m_commonShaderSourceDir,
			};

			for (auto const& technique : m_techniqueDescs)
			{
				for (uint8_t shaderTypeIdx = 0; shaderTypeIdx < re::Shader::ShaderType_Count; ++shaderTypeIdx)
				{
					if (technique.second.m_shaderNames[shaderTypeIdx].empty())
					{
						continue;
					}
				
					result = BuildShaderFile(
						glslShaderDirs, 
						technique.second.m_shaderNames[shaderTypeIdx], 
						static_cast<re::Shader::ShaderType>(shaderTypeIdx), 
						m_parseParams.m_glslShaderOutputDir);

					if (result != droid::ErrorCode::Success)
					{
						return result;
					}
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

		filewriter.OpenNamespace("effect::drawstyle");

		// Bitmasks:
		{
			filewriter.WriteLine("using Bitmask = uint64_t;");
			filewriter.EmptyLine();

			filewriter.WriteLine("constexpr Bitmask DefaultTechnique = 0;");

			uint8_t bitIdx = 0;
			for (auto const& drawstyle : m_drawstyles)
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
				result = droid::ErrorCode::DataError;
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

			for (auto const& drawstyle : m_drawstyles)
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