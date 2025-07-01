// � 2024 Adam Badke. All rights reserved.
#pragma once
#include "EffectParsing.h"


namespace droid
{
	class FileWriter final
	{
	public:
		FileWriter(std::string const& outputFilepath, std::string const& outputFileName);
		~FileWriter();

		bool HasError() const noexcept;

		void OpenNamespace(char const*);
		void OpenNamespace(std::string const&);
		void CloseNamespace();

		void EmptyLine();

		void WriteLine(char const*);
		void WriteLine(std::string const&);

		void Indent();
		void Unindent();

		void OpenBrace();	// {
		void CloseBrace();	// }

		void OpenStructBrace();		// {
		void CloseStructBrace();	// };


	private:
		bool m_hasError;
		std::ofstream m_outputStream;

		uint8_t m_curIndentLevel;
	};


	inline bool FileWriter::HasError() const noexcept
	{
		return m_hasError;
	}
}