// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "Context.h"
#include "Context_DX12.h"
#include "Context_OpenGL.h"
#include "Context_Platform.h"
#include "DebugConfiguration.h"
#include "SysInfo_Platform.h"

using std::make_shared;


namespace re
{
	Context* Context::Get()
	{
		static std::unique_ptr<re::Context> instance = std::move(re::Context::CreateSingleton());
		return instance.get();
	}


	std::unique_ptr<re::Context> Context::CreateSingleton()
	{
		std::unique_ptr<re::Context> newContext = nullptr;
		const platform::RenderingAPI& api = en::Config::Get()->GetRenderingAPI();
		switch (api)
		{
		case platform::RenderingAPI::OpenGL:
		{
			newContext.reset(new opengl::Context());
		}
		break;
		case platform::RenderingAPI::DX12:
		{
			newContext.reset(new dx12::Context());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}

		return newContext;
	}


	Context::Context()
		: m_renderDocApi(nullptr)
	{
		const bool enableRenderDocProgrammaticCaptures =
			en::Config::Get()->ValueExists(en::ConfigKeys::k_renderDocProgrammaticCapturesCmdLineArg);

		if(enableRenderDocProgrammaticCaptures)
		{
			LOG("Loading renderdoc.dll...");

			HMODULE renderDocModule = LoadLibraryA("renderdoc.dll");
			if (renderDocModule)
			{
				LOG("Successfully loaded renderdoc.dll");

				pRENDERDOC_GetAPI RENDERDOC_GetAPI = 
					(pRENDERDOC_GetAPI)GetProcAddress(renderDocModule, "RENDERDOC_GetAPI");
				int result = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&m_renderDocApi);
				SEAssert("Failed to get the RenderDoc API", result == 1);

				// Set the capture options before the graphics API is initialized:
				int captureOptionResult = 
					m_renderDocApi->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_AllowVSync, 1);

				captureOptionResult =
					m_renderDocApi->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_AllowFullscreen, 1);

				// Don't capture callstacks (for now)
				captureOptionResult =
					m_renderDocApi->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_CaptureCallstacks, 0);

				captureOptionResult =
					m_renderDocApi->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_CaptureCallstacksOnlyActions, 0);

				if (en::Config::Get()->GetValue<int>(en::ConfigKeys::k_debugLevelCmdLineArg) >= 1)
				{
					captureOptionResult =
						m_renderDocApi->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_APIValidation, 1);

					captureOptionResult =
						m_renderDocApi->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_VerifyBufferAccess, 1);
				}

				// Only include resources necessary for the final capture (for now)
				captureOptionResult =
					m_renderDocApi->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_RefAllResources, 0);

				// Set the default output folder/file path. RenderDoc appends "_frameXYZ.rdc" to the end
				std::string const& renderDocCapturePath = std::format("{}\\{}\\{}_{}_{}",
					en::Config::Get()->GetValueAsString(en::ConfigKeys::k_documentsFolderPathKey),
					en::ConfigKeys::k_renderDocCaptureFolderName,
					en::ConfigKeys::k_captureTitle,
					en::Config::RenderingAPIToCStr(en::Config::Get()->GetRenderingAPI()),
					util::GetTimeAndDateAsString());
				m_renderDocApi->SetCaptureFilePathTemplate(renderDocCapturePath.c_str());
			}
			else
			{
				const HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
				const _com_error comError(hr);
				LOG_ERROR(std::format("HRESULT error loading RenderDoc module: \"{}\"", comError.ErrorMessage()).c_str());
			}
		}
	}


	void Context::Destroy()
	{
		m_swapChain.Destroy();
		platform::Context::Destroy(*this);
	}
}