// © 2022 Adam Badke. All rights reserved.
#include "GraphicsSystem.h"
#include "LogManager.h"
#include "TextUtils.h"


namespace gr
{
	GraphicsSystem::GraphicsSystem(char const* name, gr::GraphicsSystemManager* owningGSM)
		: NamedObject(name)
		, m_graphicsSystemManager(owningGSM)
	{
		LOG("Creating %s", name);
	}


	void GraphicsSystem::ShowImGuiWindow()
	{
		ImGui::Text("...");
	}



	// Static scriptable rendering pipeline enrollment helpers:
	//------------------------------------------------------------------------------------------------------------------

	std::unique_ptr<std::map<std::string, GraphicsSystem::CreateFn>> GraphicsSystem::s_gsCreateFunctions = nullptr;
	std::unique_ptr<std::mutex> GraphicsSystem::s_gsCreateFunctionsMutex = nullptr;


	bool GraphicsSystem::RegisterGS(char const* scriptName, CreateFn createFn)
	{
		// Our auto-registration mechanism uses the initialization of static variables to trigger calls to this function
		// during app startup, before main() is called. As the calls come from different translation units, we cannot
		// guarantee any other static variables (e.g. s_gsCreateFunctions or s_gsCreateFunctionsMutex) have been
		// initialized yet. So we use something akin to the construct-on-first-use idiom here, ensuring the variables
		// are initialized before their first access, regardless of the compiler's chosen static initialization order.
		static bool s_firstExecution = true;
		if (s_firstExecution)
		{
			s_firstExecution = false;
			s_gsCreateFunctionsMutex = std::make_unique<std::mutex>();
			s_gsCreateFunctions = std::make_unique<std::map<std::string, GraphicsSystem::CreateFn>>();
		}


		std::lock_guard<std::mutex> lock(*s_gsCreateFunctionsMutex);
		
		std::string lowercaseScriptName(util::ToLower(scriptName));
		if (!s_gsCreateFunctions->contains(lowercaseScriptName))
		{
			s_gsCreateFunctions->emplace(std::move(lowercaseScriptName), createFn);
			return true;
		}

		return false;
	}


	std::unique_ptr<gr::GraphicsSystem> GraphicsSystem::CreateByName(
		char const* scriptName, gr::GraphicsSystemManager* gsm)
	{
		std::string const& lowercaseScriptName(util::ToLower(scriptName));

		CreateFn gsCreateFunction = nullptr;
		{
			std::lock_guard<std::mutex> lock(*s_gsCreateFunctionsMutex);

			auto createFnItr = s_gsCreateFunctions->find(lowercaseScriptName);
			if (createFnItr != s_gsCreateFunctions->end())
			{
				gsCreateFunction = createFnItr->second;
			}
		}
		
		SEAssert(gsCreateFunction, "Graphics system name not found. Creation failed");

		if (gsCreateFunction)
		{
			return gsCreateFunction(gsm);
		}		

		return nullptr;
	}


	std::unique_ptr<gr::GraphicsSystem> GraphicsSystem::CreateByName(
		std::string const& scriptName, gr::GraphicsSystemManager* gsm)
	{
		return CreateByName(scriptName.c_str(), gsm);
	}
}