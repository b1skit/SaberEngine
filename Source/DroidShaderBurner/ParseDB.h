// © 2024 Adam Badke. All rights reserved.
#pragma once


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
		std::string m_cppCodeGenOutputDir;
		std::string m_hlslCodeGenOutputDir;
		std::string m_glslCodeGenOutputDir;

		// File names:
		std::string m_effectManifestFileName;
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


	public:
		void AddDrawstyle(std::string const& ruleName, std::string const& mode);

		struct VertexStreamSlotDesc
		{
			std::string m_dataType;
			std::string m_name;
			std::string m_semantic;
		};
		void AddVertexStreamSlot(std::string const& streamBlockName, VertexStreamSlotDesc&&);


	private: // Parsing:
		time_t GetMostRecentlyModifiedFileTime(std::string const& filesystemTarget);

		droid::ErrorCode ParseEffectFile(std::string const& effectName, ParseParams const&);


	private:
		ParseParams m_parseParams;
		
		std::map<std::string, std::set<std::string>> m_drawstyles;
		std::map<std::string, std::vector<VertexStreamSlotDesc>> m_vertexStreamDescs;


	private: // Code gen:
		static constexpr char const* m_drawstyleHeaderFilename = "DrawStyles.h";
		droid::ErrorCode GenerateCPPCode_Drawstyle() const;

		static constexpr char const* m_vertexStreamsFilenamePrefex = "VertexStreams_"; // e.g. VertexStreams_Default.hlsli
		droid::ErrorCode GenerateShaderCode_VertexStreams() const;
	};


	inline void ParseDB::AddDrawstyle(std::string const& ruleName, std::string const& modeName)
	{
		if (!m_drawstyles.contains(ruleName)) // If this is the first drawstyle rule we've seen, add a new set of modes
		{
			std::cout << "Found new drawstyle:\t\t{\"Rule:\" : \"" << ruleName.c_str() 
				<< "\", \"Mode:\": \"" << modeName.c_str() << "\"}\n";

			m_drawstyles.emplace(ruleName, std::set<std::string>{modeName});
		}
		else if (!m_drawstyles.at(ruleName).contains(modeName))
		{
			std::cout << "Added new drawstyle mode:\t{\"Rule:\" : \"" << ruleName.c_str()
				<< "\", \"Mode:\": \"" << modeName.c_str() << "\"}\n";

			m_drawstyles.at(ruleName).emplace(modeName);
		}
	}
}