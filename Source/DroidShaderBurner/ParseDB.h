// � 2024 Adam Badke. All rights reserved.
#pragma once
#include "ParseHelpers.h"

#include "Core/Util/FileIOUtils.h"


namespace droid
{
	struct ParseParams
	{
		bool m_allowJSONExceptions = true;
		bool m_ignoreJSONComments = true;

		// Paths:
		std::string m_projectRootDir;
		std::string m_runtimeAppDir;
		std::string m_effectSourceDir;

		// Dependencies:
		std::string m_directXCompilerExePath;

		// Input paths:
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

		std::string m_runtimeEffectsDir; // Runtime version of Effect jsons

		// File names:
		std::string m_effectManifestFileName;

		// Platform-specific args:
		std::string m_dx12TargetProfile;

		util::BuildConfiguration m_buildConfiguration;

		bool m_doCppCodeGen = true;
		bool m_compileShaders = true;
		bool m_useDXCApi = true; // Use the C++ DXC API by default, unless a dxc.exe path is provided
	};


	class ParseDB final
	{
	public:
		ParseDB(ParseParams const&);

		~ParseDB() = default;
		ParseDB(ParseDB const&) = default;
		ParseDB(ParseDB&&) noexcept = default;
		ParseDB& operator=(ParseDB const&) = default;
		ParseDB& operator=(ParseDB&&) noexcept = default;


	public:
		bool Parse();

		bool GenerateCPPCode() const;
		bool GenerateShaderCode() const;
		bool CompileShaders() const;


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

		void AddTechnique(std::string const& owningEffectName, TechniqueDesc&&);
		bool HasTechnique(std::string const& effectName, std::string const& techniqueName) const;
		TechniqueDesc const& GetTechnique(std::string const& effectName, std::string const& techniqueName) const;


	private: // Parsing:
		void ParseEffectFile(std::string const& effectName, ParseParams const&);

		void PostProcessEffectTechniques(nlohmann::json& effectJSON, std::string const& effectName);

		void WriteRuntimeEffectFile(auto const& effectJSON, std::string const& effectFileName);


		// Helper: Add an entry to the unique rule -> mode mapping (m_drawStyleRuleToModes)
		void AddDrawStyleRuleMode(std::string const& ruleName, std::string const& mode);


	private:
		ParseParams m_parseParams;
		
		std::map<std::string, std::set<std::string>> m_drawStyleRuleToModes; // All seen rules/modes
		std::map<std::string, std::vector<DrawStyleTechnique>> m_effectToDrawStyleTechnique; // For resolving shader permutations
		// TODO: Use this. For now, we just record the data without using it

		std::map<std::string, std::vector<VertexStreamSlotDesc>> m_vertexStreamDescs;
		std::map<std::string, std::map<std::string, TechniqueDesc>> m_effectTechniqueDescs;


	private: // Code gen:
		static constexpr char const* m_drawstyleHeaderFilename = "DrawStyles.h";
		void GenerateCPPCode_Drawstyle() const;

		static constexpr char const* m_vertexStreamsFilenamePrefex = "VertexStreams_"; // e.g. VertexStreams_Default.hlsli
		void GenerateShaderCode_VertexStreams() const;
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


	inline void ParseDB::AddTechnique(std::string const& owningEffectName, TechniqueDesc&& techniqueDesc)
	{
		if (!m_effectTechniqueDescs.contains(owningEffectName))
		{
			m_effectTechniqueDescs.emplace(owningEffectName, std::map<std::string, TechniqueDesc>());
		}
		else if (m_effectTechniqueDescs.at(owningEffectName).contains(techniqueDesc.Name))
		{
			std::cout << "Error: Adding Technique " << techniqueDesc.Name.c_str() << ", and a Technique with that name "
				"already exists. Technique names must be unique per Effect.\n";
			throw droid::JSONException("Technique name already exists: " + techniqueDesc.Name);
		}

		std::cout << "Adding Technique \"" << techniqueDesc.Name.c_str() << "\"\n";

		m_effectTechniqueDescs.at(owningEffectName).emplace(techniqueDesc.Name, std::move(techniqueDesc));
	}


	inline bool ParseDB::HasTechnique(std::string const& effectName, std::string const& techniqueName) const
	{
		if (m_effectTechniqueDescs.contains(effectName))
		{
			return m_effectTechniqueDescs.at(effectName).contains(techniqueName);
		}
		return false;
	}


	inline TechniqueDesc const& ParseDB::GetTechnique(
		std::string const& effectName, std::string const& techniqueName) const
	{
		return m_effectTechniqueDescs.at(effectName).at(techniqueName);
	}
}