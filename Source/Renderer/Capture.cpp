// © 2025 Adam Badke. All rights reserved.
#include "Capture.h"
#include "Context.h"
#include "EnumTypes.h"
#include "Debug_DX12.h"

#include "Core/Config.h"
#include "Core/Logger.h"


namespace re
{
	re::Context* ICapture::s_context = nullptr;


	// ---


	RenderDocCapture::RenderDocAPI* RenderDocCapture::InitializeRenderDocAPI(platform::RenderingAPI api)
	{
		LOG("Loading renderdoc.dll...");

		RenderDocAPI* renderDocAPI = nullptr;

		HMODULE renderDocModule = LoadLibraryA("renderdoc.dll");
		if (renderDocModule)
		{
			LOG("Successfully loaded renderdoc.dll");

			pRENDERDOC_GetAPI RENDERDOC_GetAPI =
				(pRENDERDOC_GetAPI)GetProcAddress(renderDocModule, "RENDERDOC_GetAPI");
			int result = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&renderDocAPI);
			SEAssert(result == 1, "Failed to get the RenderDoc API");

			// Set the capture options before the graphics API is initialized:
			int captureOptionResult =
				renderDocAPI->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_AllowVSync, 1);

			captureOptionResult =
				renderDocAPI->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_AllowFullscreen, 1);

			// Don't capture callstacks (for now)
			captureOptionResult =
				renderDocAPI->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_CaptureCallstacks, 0);

			captureOptionResult =
				renderDocAPI->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_CaptureCallstacksOnlyActions, 0);

			if (core::Config::GetValue<int>(core::configkeys::k_debugLevelCmdLineArg) >= 1)
			{
				captureOptionResult =
					renderDocAPI->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_APIValidation, 1);

				captureOptionResult =
					renderDocAPI->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_VerifyBufferAccess, 1);

				LOG("RenderDoc API Validation and buffer access verification enabled");
			}

			// Only include resources necessary for the final capture (for now)
			captureOptionResult =
				renderDocAPI->SetCaptureOptionU32(RENDERDOC_CaptureOption::eRENDERDOC_Option_RefAllResources, 0);

			// Set the default output folder/file path. RenderDoc appends "_frameXYZ.rdc" to the end
			std::string const& renderDocCapturePath = std::format("{}\\{}\\{}_{}_{}",
				core::Config::GetValueAsString(core::configkeys::k_documentsFolderPathKey),
				core::configkeys::k_renderDocCaptureFolderName,
				core::configkeys::k_captureTitle,
				platform::RenderingAPIToCStr(api),
				util::GetTimeAndDateAsString());
			renderDocAPI->SetCaptureFilePathTemplate(renderDocCapturePath.c_str());
		}
		else
		{
			const HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
			const _com_error comError(hr);
			LOG_ERROR(std::format("HRESULT error loading RenderDoc module: \"{}\"",
				util::FromWideCString(comError.ErrorMessage())).c_str());
		}

		return renderDocAPI;
	}


	void RenderDocCapture::RequestGPUCapture(uint32_t numFrames)
	{
		SEAssert(s_context, "Context cannot be null");

		re::Context::RenderDocAPI* renderDocApi = s_context->GetRenderDocAPI();

		const bool renderDocCmdLineEnabled =
			core::Config::KeyExists(core::configkeys::k_renderDocProgrammaticCapturesCmdLineArg) &&
			renderDocApi != nullptr;

		if (renderDocCmdLineEnabled)
		{
			int major, minor, patch;
			renderDocApi->GetAPIVersion(&major, &minor, &patch);
			LOG(std::format("Requesting capture from Renderdoc API {}.{}.{}", major, minor, patch));
			
			std::unique_ptr<RenderDocCapture> newCapture(new RenderDocCapture(numFrames));

			s_context->RequestCapture(std::move(newCapture));
		}
		else
		{
			LOG_ERROR(std::format("RenderDoc captures not enabled. Ensure you launched with \"-{}\"",
				core::configkeys::k_renderDocProgrammaticCapturesCmdLineArg));
		}
	}


	RenderDocCapture::RenderDocCapture(uint32_t numFrames)
		: m_numFrames(numFrames)
	{
	}


	bool RenderDocCapture::TriggerCaptureInternal()
	{
		SEAssert(s_context, "Context cannot be null");

		re::Context::RenderDocAPI* renderDocApi = s_context->GetRenderDocAPI();

		renderDocApi->TriggerMultiFrameCapture(m_numFrames);

		return true;
	}


	void RenderDocCapture::ShowImguiWindow()
	{
		if (ImGui::CollapsingHeader("RenderDoc"))
		{
			ImGui::Indent();

			re::Context::RenderDocAPI* renderDocApi = s_context->GetRenderDocAPI();

			const bool renderDocCmdLineEnabled =
				core::Config::KeyExists(core::configkeys::k_renderDocProgrammaticCapturesCmdLineArg) &&
				renderDocApi != nullptr;

			if (!renderDocCmdLineEnabled)
			{
				ImGui::Text(std::format("Launch with -{} to enable",
					core::configkeys::k_renderDocProgrammaticCapturesCmdLineArg).c_str());
			}
			else
			{
				int major, minor, patch;
				renderDocApi->GetAPIVersion(&major, &minor, &patch);
				ImGui::Text(std::format("Renderdoc API {}.{}.{}", major, minor, patch).c_str());

				if (ImGui::CollapsingHeader("View capture options"))
				{
					ImGui::Indent();

					ImGui::Text(std::format("Allow VSync: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_AllowVSync)).c_str());

					ImGui::Text(std::format("Allow fullscreen: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_AllowFullscreen)).c_str());

					ImGui::Text(std::format("API validation: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_APIValidation)).c_str());

					ImGui::Text(std::format("Capture callstacks: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_CaptureCallstacks)).c_str());

					ImGui::Text(std::format("Only capture callstacks for actions: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_CaptureCallstacksOnlyActions)).c_str());

					ImGui::Text(std::format("Debugger attach delay: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_DelayForDebugger)).c_str());

					ImGui::Text(std::format("Verify buffer access: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_VerifyBufferAccess)).c_str());

					ImGui::Text(std::format("Hook into child processes: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_HookIntoChildren)).c_str());

					ImGui::Text(std::format("Reference all resources: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_RefAllResources)).c_str());

					ImGui::Text(std::format("Capture all command lists from start: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_CaptureAllCmdLists)).c_str());

					ImGui::Text(std::format("Mute API debugging output: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_DebugOutputMute)).c_str());

					ImGui::Text(std::format("Allow unsupported vendor extensions: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_AllowUnsupportedVendorExtensions)).c_str());

					ImGui::Text(std::format("Soft memory limit: {}",
						renderDocApi->GetCaptureOptionU32(eRENDERDOC_Option_SoftMemoryLimit)).c_str());

					ImGui::Unindent();
				}
				if (ImGui::CollapsingHeader("Configure overlay"))
				{
					ImGui::Indent();
					const uint32_t overlayBits = renderDocApi->GetOverlayBits();

					static bool s_overlayEnabled = (overlayBits & RENDERDOC_OverlayBits::eRENDERDOC_Overlay_Enabled);
					ImGui::Checkbox("Display overlay?", &s_overlayEnabled);

					static bool s_overlayFramerate = (overlayBits & RENDERDOC_OverlayBits::eRENDERDOC_Overlay_FrameRate);
					ImGui::Checkbox("Frame rate", &s_overlayFramerate);

					static bool s_overlayFrameNum = (overlayBits & RENDERDOC_OverlayBits::eRENDERDOC_Overlay_FrameNumber);
					ImGui::Checkbox("Frame number", &s_overlayFrameNum);

					static bool s_overlayCaptureList = (overlayBits & RENDERDOC_OverlayBits::eRENDERDOC_Overlay_CaptureList);
					ImGui::Checkbox("Recent captures", &s_overlayCaptureList);

					renderDocApi->MaskOverlayBits(
						0,
						(s_overlayEnabled ? eRENDERDOC_Overlay_Enabled : 0) |
						(s_overlayFramerate ? eRENDERDOC_Overlay_FrameRate : 0) |
						(s_overlayFrameNum ? eRENDERDOC_Overlay_FrameNumber : 0) |
						(s_overlayCaptureList ? eRENDERDOC_Overlay_CaptureList : 0));

					ImGui::Unindent();
				}

				static char s_renderDocCaptureDir[256];
				static bool s_loadedPath = false;
				if (!s_loadedPath)
				{
					s_loadedPath = true;
					memcpy(s_renderDocCaptureDir,
						renderDocApi->GetCaptureFilePathTemplate(),
						strlen(renderDocApi->GetCaptureFilePathTemplate()) + 1);
				}

				if (ImGui::InputText("Output path & prefix", s_renderDocCaptureDir, IM_ARRAYSIZE(s_renderDocCaptureDir)))
				{
					renderDocApi->SetCaptureFilePathTemplate(s_renderDocCaptureDir);
				}

				static int s_numRenderDocFrames = 1;
				if (ImGui::Button("Capture RenderDoc Frame"))
				{
					RenderDocCapture::RequestGPUCapture(static_cast<uint32_t>(s_numRenderDocFrames));
				}
				ImGui::SliderInt("No. of frames", &s_numRenderDocFrames, 1, 10);
			}
			ImGui::Unindent();
		}
	}


	// ---


	HMODULE PIXCapture::InitializePIXCPUCaptureModule()
	{
		LOG("Loading DX12 PIX CPU programmatic capture module");
		HMODULE pixCPUCaptureModule = PIXLoadLatestWinPixTimingCapturerLibrary();

		if (pixCPUCaptureModule == nullptr)
		{
			const HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
			dx12::CheckHResult(hr, "Failed to create PIX CPU capture module");
		}
		return pixCPUCaptureModule;
	}


	HMODULE PIXCapture::InitializePIXGPUCaptureModule()
	{
		LOG("Loading DX12 PIX GPU programmatic capture module");
		HMODULE pixGPUCaptureModule = PIXLoadLatestWinPixGpuCapturerLibrary(); // This must be done before loading any D3D12 APIs

		if (pixGPUCaptureModule == nullptr)
		{
			const HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
			dx12::CheckHResult(hr, "Failed to create PIX GPU capture module");
		}
		return pixGPUCaptureModule;
	}


	void PIXCapture::RequestGPUCapture(uint32_t numFrames, std::string const& captureOutputDirectory)
	{
		const platform::RenderingAPI renderingAPI =
			core::Config::GetValue<platform::RenderingAPI>(core::configkeys::k_renderingAPIKey);

		const bool isDX12 = renderingAPI == platform::RenderingAPI::DX12;
		const bool pixGPUCaptureCmdLineEnabled = isDX12 &&
			core::Config::KeyExists(core::configkeys::k_pixGPUProgrammaticCapturesCmdLineArg);
		const bool pixCPUCaptureCmdLineEnabled = isDX12 &&
			core::Config::KeyExists(core::configkeys::k_pixCPUProgrammaticCapturesCmdLineArg);

		if (!pixGPUCaptureCmdLineEnabled && !pixCPUCaptureCmdLineEnabled)
		{
			LOG_ERROR(std::format("PIX captures not enabled. Ensure you launched with \"-{}\" or \"-{}\", "
				"run PIX in administrator mode, and attach to the current process.",
				core::configkeys::k_pixGPUProgrammaticCapturesCmdLineArg,
				core::configkeys::k_pixCPUProgrammaticCapturesCmdLineArg));
		}
		else
		{
			std::unique_ptr<PIXCapture> newCapture(new PIXCapture(numFrames, std::string(captureOutputDirectory)));
			s_context->RequestCapture(std::move(newCapture));
		}
	}


	void PIXCapture::RequestCPUCapture(
		PIXCPUCaptureSettings const& captureSettings, std::string const& captureOutputDirectory)
	{
		const platform::RenderingAPI renderingAPI =
			core::Config::GetValue<platform::RenderingAPI>(core::configkeys::k_renderingAPIKey);

		const bool isDX12 = renderingAPI == platform::RenderingAPI::DX12;
		const bool pixGPUCaptureCmdLineEnabled = isDX12 &&
			core::Config::KeyExists(core::configkeys::k_pixGPUProgrammaticCapturesCmdLineArg);
		const bool pixCPUCaptureCmdLineEnabled = isDX12 &&
			core::Config::KeyExists(core::configkeys::k_pixCPUProgrammaticCapturesCmdLineArg);

		if (!pixGPUCaptureCmdLineEnabled && !pixCPUCaptureCmdLineEnabled)
		{
			LOG_ERROR(std::format("PIX captures not enabled. Ensure you launched with \"-{}\" or \"-{}\", "
				"run PIX in administrator mode, and attach to the current process.",
				core::configkeys::k_pixGPUProgrammaticCapturesCmdLineArg,
				core::configkeys::k_pixCPUProgrammaticCapturesCmdLineArg));
		}
		else
		{
			std::unique_ptr<PIXCapture> newCapture(new PIXCapture(captureSettings, std::string(captureOutputDirectory)));
			s_context->RequestCapture(std::move(newCapture));
		}
	}


	PIXCapture::PIXCapture(uint32_t numFrames, std::string&& captureOutputDir)
		: m_captureOutputDirectory(std::forward<std::string>(captureOutputDir))
		, m_numGPUFrames(numFrames)
		, m_type(CaptureType::GPU)
	{
		if (!std::filesystem::exists(m_captureOutputDirectory))
		{
			std::filesystem::create_directories(m_captureOutputDirectory);
		}
	}


	PIXCapture::PIXCapture(PIXCPUCaptureSettings const& captureSettings, std::string&& captureOutputDir)
		: m_captureOutputDirectory(std::forward<std::string>(captureOutputDir))
		, m_cpuCaptureSettings(captureSettings)
		, m_type(CaptureType::CPU)
	{
		if (!std::filesystem::exists(m_captureOutputDirectory))
		{
			std::filesystem::create_directories(m_captureOutputDirectory);
		}
	}


	PIXCapture::~PIXCapture()
	{
		m_cpuCaptureTimer.Stop();
	}
	

	bool PIXCapture::CaptureIsComplete()
	{
		switch (m_type)
		{
		case CaptureType::CPU:
		{
			if (m_cpuCaptureTimer.PeekSec() >= m_cpuCaptureSettings.m_captureTimeSec)
			{
				m_cpuCaptureTimer.Stop();
				const HRESULT timingCaptureEndResult = PIXEndCapture(false);
				if (timingCaptureEndResult != S_OK)
				{
					const _com_error comError(timingCaptureEndResult);
					const std::string errorMessage = util::FromWideCString(comError.ErrorMessage());

					LOG_ERROR("Failed to end PIX timing capture: \"%s\"", errorMessage.c_str());
				}
				return true;
			}
			else
			{
				return false;
			}
		}
		break;
		case CaptureType::GPU:
		{
			return true; // GPU captures are triggered immediately
		}
		break;
		default: SEAssertF("Invalid PIX capture type");
		}
		return true; // This should never happen
	}


	bool PIXCapture::TriggerCaptureInternal()
	{
		std::wstring const& filepath = util::ToWideString(
			std::format("{}\\{}GPUCapture_{}.wpix",
				m_captureOutputDirectory,
				core::configkeys::k_captureTitle,
				util::GetTimeAndDateAsString()));

		switch (m_type)
		{
		case CaptureType::CPU:
		{
			// For compatibility with Xbox, captureFlags must be set to PIX_CAPTURE_GPU or PIX_CAPTURE_TIMING 
			// otherwise the function will return E_NOTIMPL
			const DWORD captureFlags = PIX_CAPTURE_TIMING;

			PIXCaptureParameters pixCaptureParams = PIXCaptureParameters{
				.TimingCaptureParameters{
					.FileName = filepath.c_str(),

					.MaximumToolingMemorySizeMb = 0, // Ignored on PIX for Windows
					.CaptureStorage{}, // Ignored on PIX for Windows

					.CaptureGpuTiming = m_cpuCaptureSettings.m_captureGPUTimings,

					.CaptureCallstacks = m_cpuCaptureSettings.m_captureCallstacks,
					.CaptureCpuSamples = m_cpuCaptureSettings.m_captureCpuSamples,
					.CpuSamplesPerSecond = m_cpuCaptureSettings.m_cpuSamplesPerSecond,

					.CaptureFileIO = m_cpuCaptureSettings.m_captureFileIO,

					.CaptureVirtualAllocEvents = m_cpuCaptureSettings.m_captureVirtualAllocEvents,
					.CaptureHeapAllocEvents = m_cpuCaptureSettings.m_captureHeapAllocEvents,
					.CaptureXMemEvents = false, // Xbox only
					.CapturePixMemEvents = false // Xbox only
				}
			};

			const HRESULT timingCaptureStartResult = PIXBeginCapture(captureFlags, &pixCaptureParams);
			if (timingCaptureStartResult == S_OK)
			{
				m_cpuCaptureTimer.Start();
			}
			else
			{
				const _com_error comError(timingCaptureStartResult);
				const std::string errorMessage = std::format("HRESULT error \"{}\" starting PIX timing capture.\n"
					"Is PIX running in administrator mode, and attached to the process? Is only 1 command line "
					"argument supplied?",
					util::FromWideCString(comError.ErrorMessage()));

				LOG_ERROR("Failed to start PIX timing capture: \"%s\"", errorMessage.c_str());

				return false;
			}
		}
		break;
		case CaptureType::GPU:
		{
			const HRESULT gpuHRESULT = ::PIXGpuCaptureNextFrames(filepath.c_str(), m_numGPUFrames);
			if (gpuHRESULT != S_OK)
			{
				const _com_error comError(gpuHRESULT);
				const std::string errorMessage = std::format("HRESULT error \"{}\" starting PIX GPU capture.\n"
					"Is PIX running in administrator mode, and attached to the process? Is only 1 command line "
					"argument supplied?",
					util::FromWideCString(comError.ErrorMessage()));

				LOG_ERROR("Failed to start PIX GPU capture: \"%s\"", errorMessage.c_str());

				return false;
			}
		}
		break;
		default: SEAssertF("Invalid PIX capture type");
		}
		
		return true;
	}


	void PIXCapture::ShowImguiWindow()
	{
		if (ImGui::CollapsingHeader("PIX Captures")) // https://devblogs.microsoft.com/pix/programmatic-capture/
		{
			ImGui::Indent();

			const platform::RenderingAPI renderingAPI =
				core::Config::GetValue<platform::RenderingAPI>(core::configkeys::k_renderingAPIKey);

			const bool isDX12 = renderingAPI == platform::RenderingAPI::DX12;
			const bool pixGPUCaptureCmdLineEnabled = isDX12 &&
				core::Config::KeyExists(core::configkeys::k_pixGPUProgrammaticCapturesCmdLineArg);
			const bool pixCPUCaptureCmdLineEnabled = isDX12 &&
				core::Config::KeyExists(core::configkeys::k_pixCPUProgrammaticCapturesCmdLineArg);

			if (!pixGPUCaptureCmdLineEnabled && !pixCPUCaptureCmdLineEnabled)
			{
				ImGui::Text(std::format("Launch with -{} or -{} to enable.\n"
					"Run PIX in administrator mode, and attach to the current process.",
					core::configkeys::k_pixGPUProgrammaticCapturesCmdLineArg,
					core::configkeys::k_pixCPUProgrammaticCapturesCmdLineArg).c_str());
			}

			// GPU captures:
			ImGui::BeginDisabled(!pixGPUCaptureCmdLineEnabled);		
			{
				static char s_pixGPUCapturePath[256];
				static bool loadedPath = false;
				if (!loadedPath)
				{
					loadedPath = true;
					std::string const& pixFilePath = std::format("{}\\{}\\",
						core::Config::GetValueAsString(core::configkeys::k_documentsFolderPathKey),
						core::configkeys::k_pixCaptureFolderName);
					memcpy(s_pixGPUCapturePath, pixFilePath.c_str(), pixFilePath.length() + 1);
				}

				ImGui::InputText("Output path", s_pixGPUCapturePath, IM_ARRAYSIZE(s_pixGPUCapturePath));

				static int s_numPixGPUCaptureFrames = 1;
				ImGui::SliderInt("No. of frames", &s_numPixGPUCaptureFrames, 1, 10);

				if (ImGui::Button("Capture PIX GPU Frame"))
				{
					PIXCapture::RequestGPUCapture(s_numPixGPUCaptureFrames, s_pixGPUCapturePath);
				}
				ImGui::SetItemTooltip("PIX must be run in administrator mode, and already attached to the process");
			}
			ImGui::EndDisabled();

			ImGui::Separator();

			// CPU timing captures:
			ImGui::BeginDisabled(!pixCPUCaptureCmdLineEnabled);
			{
				static char s_pixCPUCapturePath[256]{'\0'};
				static bool loadedPath = false;
				if (!loadedPath)
				{
					loadedPath = true;
					std::string const& pixFilePath = std::format("{}\\{}\\",
						core::Config::GetValueAsString(core::configkeys::k_documentsFolderPathKey),
						core::configkeys::k_pixCaptureFolderName);
					memcpy(s_pixCPUCapturePath, pixFilePath.c_str(), pixFilePath.length() + 1);
				}

				static re::PIXCapture::PIXCPUCaptureSettings s_pixCpuCaptureSettings{};

				static int s_cpuSamplesPerSecondSelectionIdx = 0;
				constexpr uint32_t k_cpuSamplesPerSecond[3] = { 1000u, 4000u, 8000u };

				ImGui::Text("CPU");

				ImGui::Checkbox("CPU samples", &s_pixCpuCaptureSettings.m_captureCpuSamples);

				ImGui::BeginDisabled(!s_pixCpuCaptureSettings.m_captureCpuSamples);
				if (ImGui::Combo("CPU sampling rate (/sec)", &s_cpuSamplesPerSecondSelectionIdx,
					"1000\0"
					"4000\0"
					"8000\0\0"))
				{
					s_pixCpuCaptureSettings.m_cpuSamplesPerSecond = k_cpuSamplesPerSecond[s_cpuSamplesPerSecondSelectionIdx];
				}
				ImGui::EndDisabled();

				ImGui::Checkbox("Callstacks on context switches", &s_pixCpuCaptureSettings.m_captureCallstacks);
				ImGui::Checkbox("File accesses", &s_pixCpuCaptureSettings.m_captureFileIO);
				ImGui::Checkbox("GPU timings", &s_pixCpuCaptureSettings.m_captureGPUTimings);

				ImGui::SliderFloat("Capture time (sec)", &s_pixCpuCaptureSettings.m_captureTimeSec, 0.1f, 60.f);

				if (ImGui::Button("Capture PIX CPU Timings"))
				{
					PIXCapture::RequestCPUCapture(s_pixCpuCaptureSettings, s_pixCPUCapturePath);
				}
			}
			ImGui::EndDisabled();

			ImGui::Unindent();
		}
	}
}