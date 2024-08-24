// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "EffectParsing.h"


namespace droid
{
	class FileWriter
	{
	public:
		FileWriter(std::string const& outputFilepath, std::string const& outputFileName);
		~FileWriter();

		droid::ErrorCode GetStatus() const;

		void OpenNamespace(char const*);
		void OpenNamespace(std::string const&);
		void CloseNamespace();

		void WriteLine(char const*);
		void WriteLine(std::string const&);

		void Indent();
		void Unindent();

		void OpenBrace();
		void CloseBrace();


	private:
		droid::ErrorCode m_currentStatus;
		std::ofstream m_outputStream;

		uint8_t m_curIndentLevel;
	};


	inline droid::ErrorCode FileWriter::GetStatus() const
	{
		return m_currentStatus;
	}
}