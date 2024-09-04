// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Renderer/Shader.h"

#include "Core/Util/FileIOUtils.h"


namespace droid
{
	enum ErrorCode;

	struct ParseParams
	{
		bool m_allowJSONExceptions = false;
		bool m_ignoreJSONComments = true;

		// Paths:
		std::string m_projectRootDir;
		std::string m_appDir;
		std::string m_effectsDir;

		// Dependencies:
		std::string m_directXCompilerExePath;

		// Shader input paths:
		std::string m_hlslShaderSourceDir;
		std::string m_glslShaderSourceDir;
		std::string m_commonShaderSourceDir;
		std::string m_dependenciesDir;

		// Output paths:
		std::string m_cppCodeGenOutputDir;

		std::string m_hlslCodeGenOutputDir;
		std::string m_hlslShaderOutputDir; // For compiled .cso files

		std::string m_glslCodeGenOutputDir; // For code generated from Effect definitions
		std::string m_glslShaderOutputDir; // For results of concatenating shader includes with shader text

		// File names:
		std::string m_effectManifestFileName;

		util::BuildConfiguration m_buildConfiguration;
	};


	class ParseDB
	{
	public:
		ParseDB(ParseParams const&);
		~ParseDB() = default;
		ParseDB(ParseDB const&) = default;
		ParseDB(ParseDB&&) = default;
		ParseDB& operator=(ParseDB const&) = default;
		ParseDB& operator=(ParseDB&&) = default;


	public:
		droid::ErrorCode Parse();


		droid::ErrorCode GenerateCPPCode() const;
		droid::ErrorCode GenerateShaderCode() const;
		droid::ErrorCode CompileShaders() const;


	public:
		struct DrawStyleTechnique
		{
			std::vector<std::pair<std::string, std::string>> m_drawStyleConditions; // <Rule, Mode>
			std::string m_techniqueName;
		};
		void AddEffectDrawStyleTechnique(std::string const& effectName, DrawStyleTechnique&&);

		
		struct VertexStreamSlotDesc
		{
			std::string m_dataType;
			std::string m_name;
			std::string m_semantic;
		};
		void AddVertexStreamSlot(std::string const& streamBlockName, VertexStreamSlotDesc&&);


		struct TechniqueDesc
		{
			std::unordered_set<std::string> m_excludedPlatforms;
			std::array<std::string, re::Shader::ShaderType_Count> m_shaderNames;
			std::array<std::string, re::Shader::ShaderType_Count> m_entryPointNames;
		};
		droid::ErrorCode AddTechnique(std::string const& techniqueName, TechniqueDesc&&);


	private: // Parsing:
		droid::ErrorCode ParseEffectFile(std::string const& effectName, ParseParams const&);

		// Helper: Add an entry to the unique rule -> mode mapping (m_drawStyleRuleToModes)
		void AddDrawStyleRuleMode(std::string const& ruleName, std::string const& mode);


	private:
		ParseParams m_parseParams;
		
		std::map<std::string, std::set<std::string>> m_drawStyleRuleToModes; // All seen rules/modes
		std::map<std::string, std::vector<DrawStyleTechnique>> m_effectToDrawStyleTechnique; // For resolving shader permutations
		// TODO: Use this. For now, we just record the data without using it

		std::map<std::string, std::vector<VertexStreamSlotDesc>> m_vertexStreamDescs;
		std::map<std::string, TechniqueDesc> m_techniqueDescs;


	private: // Code gen:
		static constexpr char const* m_drawstyleHeaderFilename = "DrawStyles.h";
		droid::ErrorCode GenerateCPPCode_Drawstyle() const;

		static constexpr char const* m_vertexStreamsFilenamePrefex = "VertexStreams_"; // e.g. VertexStreams_Default.hlsli
		droid::ErrorCode GenerateShaderCode_VertexStreams() const;
	};


	inline void ParseDB::AddDrawStyleRuleMode(std::string const& ruleName, std::string const& modeName)
	{
		if (!m_drawStyleRuleToModes.contains(ruleName)) // If this is the first drawstyle rule we've seen, add a new set of modes
		{
			std::cout << "Found new drawstyle:\t\t{\"Rule:\" : \"" << ruleName.c_str() 
				<< "\", \"Mode:\": \"" << modeName.c_str() << "\"}\n";

			m_drawStyleRuleToModes.emplace(ruleName, std::set<std::string>{modeName});
		}
		else if (!m_drawStyleRuleToModes.at(ruleName).contains(modeName))
		{
			std::cout << "Added new drawstyle mode:\t{\"Rule:\" : \"" << ruleName.c_str()
				<< "\", \"Mode:\": \"" << modeName.c_str() << "\"}\n";

			m_drawStyleRuleToModes.at(ruleName).emplace(modeName);
		}
	}


	inline void ParseDB::AddEffectDrawStyleTechnique(
		std::string const& effectName, DrawStyleTechnique&& drawStyleTechnique)
	{
		// Record the rule/mode in our unique set of rules and modes:
		for (auto const& ruleMode : drawStyleTechnique.m_drawStyleConditions)
		{
			AddDrawStyleRuleMode(ruleMode.first, ruleMode.second);
		}

		// Record the resolved Technique:
		if (!m_effectToDrawStyleTechnique.contains(effectName))
		{
			m_effectToDrawStyleTechnique.emplace(effectName, std::vector<DrawStyleTechnique>());
		}
		m_effectToDrawStyleTechnique.at(effectName).emplace_back(std::move(drawStyleTechnique));
	}


	inline void ParseDB::AddVertexStreamSlot(std::string const& streamBlockName, VertexStreamSlotDesc&& newSlotDesc)
	{
		if (!m_vertexStreamDescs.contains(streamBlockName))
		{
			std::cout << "Found new vertex stream block: \"" << streamBlockName.c_str() << "\"\n";

			m_vertexStreamDescs.emplace(streamBlockName, std::vector<VertexStreamSlotDesc>());
		}

		std::cout << "Adding slot to vertex stream block \"" << streamBlockName.c_str() << "\": \"Name\": \"" << 
			newSlotDesc.m_name.c_str() << "\"\n";

		std::vector<VertexStreamSlotDesc>& vertexStreamSlots = m_vertexStreamDescs.at(streamBlockName);
		vertexStreamSlots.emplace_back(std::move(newSlotDesc));
	}


	inline droid::ErrorCode ParseDB::AddTechnique(std::string const& techniqueName, TechniqueDesc&& techniqueDesc)
	{
		if (m_techniqueDescs.contains(techniqueName))
		{
			std::cout << "Error: Adding Technique " << techniqueName.c_str() << ", and a Technique with that name "
				"already exists. Technique names must be unique.\n";
			return droid::ErrorCode::JSONError;
		}
		else
		{
			std::cout << "Adding Technique \"" << techniqueName.c_str() << "\"\n";
		}
		m_techniqueDescs.emplace(techniqueName, std::move(techniqueDesc));

		return droid::ErrorCode::Success;
	}
}