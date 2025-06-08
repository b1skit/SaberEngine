// © 2024 Adam Badke. All rights reserved.
#include "Private/ParseHelpers.h"
#include "Private/ShaderCompile_DX12.h"

#include "Core/Util/TextUtils.h"

#include "Private/Renderer/Shader.h"


namespace
{
	constexpr size_t k_maxCmdLineArgLength = 4096; // Max = 32,767 chars, including the Unicode null terminator


	constexpr char const* k_shaderTypeDefines[] // Per-shader-type defines
	{
		// Rasterization pipeline:
		// ShaderType::Vertex:
		"SE_VERTEX_SHADER",

		// ShaderType::Geometry:
		"SE_GEOMETRY_SHADER",

		// ShaderType::Fragment:
		"SE_PIXEL_SHADER",

		// ShaderType::Hull:
		"SE_HULL_SHADER",

		// ShaderType::Domain:
		"SE_DOMAIN_SHADER",


		// Mesh shading pipeline:
		// ShaderType::Amplification:
		"SE_AMPLIFICATION_SHADER",

		// ShaderType::Mesh:
		"SE_MESH_SHADER",


		// Compute pipeline:
		// ShaderType::Compute:
		"SE_COMPUTE_SHADER",


		// Ray tracing pipeline:
		// ShaderType::HitGroup_Intersection:
		"SE_INTERSECTION_SHADER",

		// ShaderType::HitGroup_AnyHit:
		"SE_ANYHIT_SHADER",

		// ShaderType::HitGroup_ClosestHit:
		"SE_CLOSESTHIT_SHADER",

		// ShaderType::Callable:
		"SE_CALLABLE_SHADER",

		// ShaderType::RayGen:
		"SE_RAYGEN_SHADER",

		// ShaderType::Miss:
		"SE_MISS_SHADER",
	};
	static_assert(_countof(k_shaderTypeDefines) == re::Shader::ShaderType_Count);


	constexpr char const* k_dx12Flag = "SE_DX12";


	void AppendCmdLineArg(std::wstring& cmdLineArgs, wchar_t const* flag, wchar_t const* arg)
	{
		if (flag)
		{
			cmdLineArgs += L" ";
			cmdLineArgs += flag;
		}
		if (arg)
		{
			cmdLineArgs += L" ";
			cmdLineArgs += arg;
		}
	}


	void AppendCmdLineArg(std::wstring& cmdLineArgs, wchar_t const* flag, std::wstring const& arg)
	{
		return AppendCmdLineArg(cmdLineArgs, flag, arg.c_str());
	}


	void AppendCmdLineArg(std::wstring& cmdLineArgs, wchar_t const* flag, std::string const& arg)
	{
		return AppendCmdLineArg(cmdLineArgs, flag, util::ToWideString(arg));
	}


	std::string BuildTargetProfileArg(re::Shader::ShaderType shaderType, std::string const& targetProfileVersion)
	{
		switch (shaderType)
		{
		case re::Shader::Vertex: return "vs_" + targetProfileVersion;
		case re::Shader::Geometry: return "gs_" + targetProfileVersion;
		case re::Shader::Pixel: return "ps_" + targetProfileVersion;
		case re::Shader::Hull: return "hs_" + targetProfileVersion;
		case re::Shader::Domain: return "ds_" + targetProfileVersion;
		case re::Shader::Mesh: return "ms_" + targetProfileVersion;
		case re::Shader::Amplification: return "as_" + targetProfileVersion;
		case re::Shader::Compute: return "cs_" + targetProfileVersion;

		case re::Shader::HitGroup_Intersection: return "lib_" + targetProfileVersion;
		case re::Shader::HitGroup_AnyHit: return "lib_" + targetProfileVersion;
		case re::Shader::HitGroup_ClosestHit: return "lib_" + targetProfileVersion;

		case re::Shader::Callable: return "lib_" + targetProfileVersion;
		case re::Shader::RayGen: return "lib_" + targetProfileVersion;
		case re::Shader::Miss: return "lib_" + targetProfileVersion;
		default: return "INVALID_SHADER_TYPE_RECEIVED_BY_BuildTargetProfileArg";
		}
		SEStaticAssert(re::Shader::ShaderType_Count == 14, "Shader types are out of sync");
	}

	constexpr wchar_t const* GetOptimizationLevelCStr(uint8_t optimizationLevel)
	{
		switch (optimizationLevel)
		{
		case 0: return L"-O0";
		case 1: return L"-O1";
		case 2: return L"-O2";
		case 3: return L"-O3";
		default: return L"DROID_ERROR_INVALID_OPTIMIZATION_LEVEL";
		}
	}


	std::string BuildInputPath(
		std::vector<std::string> const& includeDirectories, 
		std::string const& extensionlessSrcFilename)
	{
		for (auto const& path : includeDirectories)
		{
			std::string const& absPath = path + extensionlessSrcFilename + ".hlsl";
			if (std::filesystem::exists(absPath))
			{
				return absPath;
			}
		}
		return "DROID_ERROR_FAILED_TO_ASSEMBLE_SHADER_INPUT_PATH";
	}
}

namespace droid
{
	droid::ErrorCode CompileShader_HLSL(
		std::string const& directXCompilerExePath,
		PROCESS_INFORMATION& processInfo,
		HLSLCompileOptions const& compileOptions,
		std::vector<std::string> const& includeDirectories,
		std::string const& extensionlessSrcFilename,
		uint64_t variantID,
		std::string const& entryPointName,
		re::Shader::ShaderType shaderType,
		std::vector<std::string> const& defines,
		std::string const& outputDir)
	{
		std::string const& outputFileName = std::format("{}.cso",
			BuildExtensionlessShaderVariantName(extensionlessSrcFilename, variantID));

		std::string concatenatedDefines;
		for (auto const& define : defines)
		{
			concatenatedDefines = std::format("{} {}", concatenatedDefines, define);
		}

		std::string const& outputMsg = std::format("Compiling HLSL {} shader \"{}\"{}{}\n",
			re::Shader::ShaderTypeToCStr(shaderType),
			outputFileName,
			concatenatedDefines.empty() ? "" : ", Defines =",
			concatenatedDefines);
		std::cout << outputMsg.c_str();

		droid::ErrorCode result = droid::ErrorCode::Success;

		// Build our wstrigns:
		std::wstring const& directXCompilerExePathW = util::ToWideString(directXCompilerExePath);

		// Build our command line argument buffer.
		// Note: The first command line argument must be the module name (i.e. argv[0]) in "quotations" (for spaces)
		std::wstring dxcCommandLineArgsW = L"\"" + directXCompilerExePathW + L"\"";

		// Generic configuration:
		AppendCmdLineArg(dxcCommandLineArgsW, L"-nologo", nullptr); // Suppress copyright message

		// Handle HLSLCompileOptions:
		if (compileOptions.m_disableOptimizations)
		{
			AppendCmdLineArg(dxcCommandLineArgsW, L"-Od", nullptr);
		}
		if (compileOptions.m_enableDebuggingInfo)
		{
			AppendCmdLineArg(dxcCommandLineArgsW, L"-Zi", nullptr);

			// Suppress "warning: no output provided for debug - embedding PDB in shader container"
			AppendCmdLineArg(dxcCommandLineArgsW, L"-Qembed_debug", nullptr);
		}
		if (compileOptions.m_allResourcesBound)
		{
			AppendCmdLineArg(dxcCommandLineArgsW, L"-all-resources-bound", nullptr);
		}
		if (compileOptions.m_treatWarningsAsErrors)
		{
			AppendCmdLineArg(dxcCommandLineArgsW, L"-WX", nullptr);
		}
		if (compileOptions.m_enable16BitTypes)
		{
			AppendCmdLineArg(dxcCommandLineArgsW, L"-enable-16bit-types", nullptr);
		}

		// Extra commands:
		AppendCmdLineArg(dxcCommandLineArgsW, L"-auto-binding-space", L"0"); // Enable auto library resource binding (space 0)

		if (compileOptions.m_targetProfile == "6_6")
		{
			AppendCmdLineArg(dxcCommandLineArgsW, L"-enable-payload-qualifiers", nullptr); // Enable PAQs in SM6.6
		}

		// Defines:
		AppendCmdLineArg(dxcCommandLineArgsW, L"-D", k_dx12Flag);
		AppendCmdLineArg(dxcCommandLineArgsW, L"-D", k_shaderTypeDefines[shaderType]);
		for (auto const& define : defines)
		{
			// DXC expects define arguments in the form of "-D defineName=value". If no value is specified, the define
			// will be set as 1 by default.
			// Here, we assume the first space delimits the value, and replace it with the '=' character
			std::string formattedDefine = define;
			const size_t spaceCharIdx = formattedDefine.find_first_of(' ');
			if (spaceCharIdx != std::string::npos)
			{
				formattedDefine[spaceCharIdx] = '=';
			}
			AppendCmdLineArg(dxcCommandLineArgsW, L"-D", formattedDefine);
		}

		// Include directories:
		for (auto const& include : includeDirectories)
		{
			AppendCmdLineArg(dxcCommandLineArgsW, L"-I", include);
		}

		AppendCmdLineArg(dxcCommandLineArgsW, L"-T", BuildTargetProfileArg(shaderType, compileOptions.m_targetProfile));
		AppendCmdLineArg(dxcCommandLineArgsW, GetOptimizationLevelCStr(compileOptions.m_optimizationLevel), nullptr);

		// Shader configuration:		
		AppendCmdLineArg(dxcCommandLineArgsW, L"-E", entryPointName);

		std::string const& combinedFilePath = std::format("{}{}", outputDir, outputFileName);

		AppendCmdLineArg(dxcCommandLineArgsW, L"-Fo", combinedFilePath);

		AppendCmdLineArg(dxcCommandLineArgsW, nullptr, BuildInputPath(includeDirectories, extensionlessSrcFilename));

		if (dxcCommandLineArgsW.size() >= k_maxCmdLineArgLength)
		{
			std::cout << "Command line for dxc.exe is too large: \"" << 
				util::FromWideString(dxcCommandLineArgsW).c_str() << "\"\n";
			return droid::ErrorCode::ConfigurationError;
		}

		// Copy the assembled command lines into a non-const array, as the unicode CreateProcessW may modify it
		wchar_t cmdLineArgBuffer[k_maxCmdLineArgLength]; 
		const size_t cmdLineArgsNumChars = dxcCommandLineArgsW.size();
		memcpy(cmdLineArgBuffer, 
			dxcCommandLineArgsW.data(), 
			dxcCommandLineArgsW.size() * sizeof(wchar_t) + sizeof(wchar_t)); // + null terminator
		
		// Create the dxc.exe process:
		// https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw
		STARTUPINFO startupInfo;
		memset(&startupInfo, 0, sizeof(startupInfo));
		startupInfo.cb = sizeof(startupInfo);

		memset(&processInfo, 0, sizeof(processInfo));

		const bool dxcProcessCreated = ::CreateProcessW(
			directXCompilerExePathW.c_str(),	// lpApplicationName: The .exe path
			cmdLineArgBuffer,					// lpCommandLine: Command line args
			nullptr,							// lpProcessAttributes: null = Process handle is not inheritable
			nullptr,							// lpThreadAttributes: null = Thread handle is not inheritable
			false,								// bInheritHandles: false = Handles are not inherited
			0,									// dwCreationFlags: Priority flags
			nullptr,							// lpEnvironment: null = Use parent's environment block
			nullptr,							// lpCurrentDirectory: Use parent's starting directory 
			&startupInfo,						// Pointer to STARTUPINFO structure
			&processInfo);						// Pointer to PROCESS_INFORMATION structure

		if (!dxcProcessCreated)
		{
			return droid::ErrorCode::DependencyError;
		}

		return result;
	}


	droid::ErrorCode PrintHLSLCompilerVersion(std::string const& directXCompilerExePath)
	{
		std::wstring const& directXCompilerExePathW = util::ToWideString(directXCompilerExePath);

		std::wstring dxcCommandLineArgsW = directXCompilerExePathW;
		AppendCmdLineArg(dxcCommandLineArgsW, L"--version", nullptr); // Display compiler version information

		// Copy the assembled command lines into a non-const array, as the unicode CreateProcessW may modify it
		wchar_t cmdLineArgBuffer[k_maxCmdLineArgLength];
		const size_t cmdLineArgsNumChars = dxcCommandLineArgsW.size();
		memcpy(
			cmdLineArgBuffer,
			dxcCommandLineArgsW.data(),
			dxcCommandLineArgsW.size() * sizeof(wchar_t) + sizeof(wchar_t)); // + null terminator

		// Create the dxc.exe process:
		// https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw
		STARTUPINFO startupInfo;
		memset(&startupInfo, 0, sizeof(startupInfo));
		startupInfo.cb = sizeof(startupInfo);

		PROCESS_INFORMATION processInfo;
		memset(&processInfo, 0, sizeof(processInfo));

		const bool dxcProcessCreated = ::CreateProcessW(
			directXCompilerExePathW.c_str(),	// lpApplicationName: The .exe path
			cmdLineArgBuffer,					// lpCommandLine: Command line args
			nullptr,							// lpProcessAttributes: null = Process handle is not inheritable
			nullptr,							// lpThreadAttributes: null = Thread handle is not inheritable
			false,								// bInheritHandles: false = Handles are not inherited
			0,									// dwCreationFlags: Priority flags
			nullptr,							// lpEnvironment: null = Use parent's environment block
			nullptr,							// lpCurrentDirectory: Use parent's starting directory 
			&startupInfo,						// Pointer to STARTUPINFO structure
			&processInfo);						// Pointer to PROCESS_INFORMATION structure

		if (!dxcProcessCreated)
		{
			return droid::ErrorCode::DependencyError;
		}

		WaitForSingleObject(processInfo.hProcess, INFINITE); // Wait until the process is done to maintain log ordering

		// Close process and thread handles
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);

		return droid::ErrorCode::Success;
	}
}