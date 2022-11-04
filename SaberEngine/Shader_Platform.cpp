#include <fstream>

#include "DebugConfiguration.h"
#include "CoreEngine.h"
#include "Shader.h"
#include "Shader_Platform.h"
#include "Shader_OpenGL.h"

using std::ifstream;
using std::string;
using std::vector;


namespace platform
{
	// Parameter struct object factory:
	void platform::Shader::PlatformParams::CreatePlatformParams(gr::Shader& shader)
	{
		const platform::RenderingAPI& api =
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			shader.m_platformParams = std::make_unique<opengl::Shader::PlatformParams>();
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

		return;
	}


	string platform::Shader::LoadShaderText(const string& filename)
	{
		// Assemble the full shader file path:
		string filepath = 
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("shaderDirectory") + filename;

		ifstream file;
		file.open(filepath.c_str());

		string output;
		string line;
		if (file.is_open())
		{
			while (file.good())
			{
				getline(file, line);
				output.append(line + "\n");
			}
		}
		else
		{
			#if defined(DEBUG_SHADER_SETUP_LOGGING)
				LOG_WARNING("LoadShaderFile failed: Could not open shader " + filepath);
			#endif

			return "";
		}

		return output;
	}


	void platform::Shader::InsertIncludedFiles(string& shaderText)
	{
		#if defined(DEBUG_SHADER_SETUP_LOGGING)
			LOG("Processing shader #include directives");
			bool foundInclude = false;
		#endif

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

							string includeFileName = shaderText.substr(firstQuoteIndex, lastQuoteIndex - firstQuoteIndex);

							#if defined(DEBUG_SHADER_SETUP_LOGGING)
								string includeDirective = shaderText.substr(foundIndex, endIndex - foundIndex - 1);	// - 1 to move back from the index of the last "
								LOG("Found include directive \"" + includeDirective + "\". Attempting to load file \"" + includeFileName + "\"");
							#endif							

							string includeFile = LoadShaderText(includeFileName);
							if (includeFile != "")
							{
								// Perform the insertion:
								string firstHalf = shaderText.substr(0, foundIndex);
								string secondHalf = shaderText.substr(endIndex + 1, shaderText.length() - 1);
								shaderText = firstHalf + includeFile + secondHalf;								

								#if defined(DEBUG_SHADER_SETUP_LOGGING)
									LOG("Successfully processed shader directive \"" + includeDirective + "\"");
									foundInclude = true;
								#endif	
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

		#if defined(DEBUG_SHADER_SETUP_LOGGING)
			if (foundInclude)
			{

				#if defined(DEBUG_SHADER_PRINT_FINAL_SHADER)
					LOG("Final shader text:\n" + shaderText);
				#else
					LOG("Finished processing #include directives");
				#endif
			}
			else
			{
				LOG("No #include directives processed. Shader is unchanged");
			}
		#endif
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
	void (*platform::Shader::Create)(gr::Shader& shader) = nullptr;
	void (*platform::Shader::Bind)(gr::Shader const&, bool doBind) = nullptr;
	void (*platform::Shader::SetUniform)(
		gr::Shader const& shader, 
		string const& uniformName, 
		void const* value, 
		platform::Shader::UniformType const type, 
		int const count) = nullptr;
	void (*platform::Shader::SetParameterBlock)(gr::Shader const&, re::ParameterBlock const&) = nullptr;
	void (*platform::Shader::Destroy)(gr::Shader&) = nullptr;

}