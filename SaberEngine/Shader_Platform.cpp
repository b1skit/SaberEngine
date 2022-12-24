// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Config.h"
#include "Shader.h"
#include "Shader_Platform.h"
#include "Shader_OpenGL.h"
#include "TextLoader.h"

using en::Config;
using std::ifstream;
using std::string;
using std::vector;


namespace platform
{
	// Parameter struct object factory:
	void platform::Shader::CreatePlatformParams(re::Shader& shader)
	{
		const platform::RenderingAPI& api = Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			shader.SetPlatformParams(std::make_unique<opengl::Shader::PlatformParams>());
		}
		break;
		case RenderingAPI::DX12:
		{
			SEAssertF("DX12 is not yet supported");
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	string platform::Shader::LoadShaderText(string const& filename)
	{
		// Assemble the full shader file path:
		const string filepath = Config::Get()->GetValue<string>("shaderDirectory") + filename;

		return util::LoadTextAsString(filepath);
	}


	void platform::Shader::InsertIncludedFiles(string& shaderText)
	{
		const string INCLUDE_KEYWORD = "#include";

		int foundIndex = 0;
		while (foundIndex != string::npos && foundIndex < shaderText.length())
		{
			foundIndex = (int)shaderText.find(INCLUDE_KEYWORD, foundIndex + 1);
			if (foundIndex != string::npos)
			{
				// Check we're not on a commented line:
				int checkIndex = foundIndex;
				bool foundComment = false;
				while (checkIndex >= 0 && shaderText[checkIndex] != '\n')
				{
					// TODO: Search from the beginning of the line
					// -> If we hit a "#include" substring first, we've got an include
					// -> Seach until the end of the line, to strip out any trailing //comments
					if (shaderText[checkIndex] == '/' && checkIndex > 0 && shaderText[checkIndex - 1] == '/')
					{
						foundComment = true;
						break;
					}
					checkIndex--;
				}
				if (foundComment)
				{
					continue;
				}

				int endIndex = (int)shaderText.find("\n", foundIndex + 1);
				if (endIndex != string::npos)
				{
					int firstQuoteIndex, lastQuoteIndex;
					
					firstQuoteIndex = (int)shaderText.find("\"", foundIndex + 1);
					if (firstQuoteIndex != string::npos && firstQuoteIndex > 0 && firstQuoteIndex < endIndex)
					{
						lastQuoteIndex = (int)shaderText.find("\"", firstQuoteIndex + 1);
						if (lastQuoteIndex != string::npos && lastQuoteIndex > firstQuoteIndex && lastQuoteIndex < endIndex)
						{
							firstQuoteIndex++; // Move ahead 1 element from the first quotation mark

							const string includeFileName = shaderText.substr(firstQuoteIndex, lastQuoteIndex - firstQuoteIndex);					

							string includeFile = LoadShaderText(includeFileName);
							if (includeFile != "")
							{
								// Perform the insertion:
								string firstHalf = shaderText.substr(0, foundIndex);
								string secondHalf = shaderText.substr(endIndex + 1, shaderText.length() - 1);
								shaderText = firstHalf + includeFile + secondHalf;								
							}
							else
							{
								LOG_ERROR("Could not find include file. Shader loading failed.");
								return;
							}
						}
					}
				}
			}							
		}
	}


	void platform::Shader::InsertDefines(string& shaderText, vector<string> const* shaderKeywords)
	{
		if ((int)shaderText.length() <= 0 || shaderKeywords == nullptr || (int)shaderKeywords->size() <= 0)
		{
			return;
		}

		// Find the #version directive, and insert our keywords immediately after it

		int foundIndex = (int)shaderText.find("#version", 0);
		if (foundIndex == string::npos)
		{
			foundIndex = 0;
		}
		// Find the next newline character:
		int endLine = (int)shaderText.find("\n", foundIndex + 1);

		// Assemble our #define lines:
		const string DEFINE_KEYWORD = "#define ";
		string assembledKeywords = "";
		for (int currentKeyword = 0; currentKeyword < (int)shaderKeywords->size(); currentKeyword++)
		{
			string defineLine = DEFINE_KEYWORD + shaderKeywords->at(currentKeyword) + "\n";

			assembledKeywords += defineLine;
		}

		// Insert our #define lines:
		shaderText.insert(endLine + 1, assembledKeywords);		
	}


	// platform::Shader static members:
	/**********************************/
	void (*platform::Shader::Create)(re::Shader& shader) = nullptr;
	void (*platform::Shader::Bind)(re::Shader&) = nullptr;
	void (*platform::Shader::SetUniform)(
		re::Shader& shader, 
		string const& uniformName, 
		void* value, 
		re::Shader::UniformType const type, 
		int const count) = nullptr;
	void (*platform::Shader::SetParameterBlock)(re::Shader&, re::ParameterBlock&) = nullptr;
	void (*platform::Shader::Destroy)(re::Shader&) = nullptr;
	void (*platform::Shader::LoadShaderTexts)(string const& extensionlessName, std::vector<std::string>& shaderTexts_out) = nullptr;
}