// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace droid
{
	enum ErrorCode;


	struct ParseParams
	{
		bool m_allowJSONExceptions = false;
		bool m_ignoreJSONComments = true;

		std::string m_workingDirectory;
		std::string m_effectsDir;
		std::string m_effectManifestPath;
		std::string m_codeGenPath;
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


	private:
		ParseParams m_parseParams;


		droid::ErrorCode ParseEffectFile(std::string const& effectName, ParseParams const&);

		void AddDrawstyle(std::string const& ruleName, std::string const& mode);
		std::map<std::string, std::set<std::string>> m_drawstyles;


	private:
		static constexpr char const* m_drawstyleHeaderName = "DrawStyles.h";
		droid::ErrorCode GenerateDrawstyleCPPCode() const;

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