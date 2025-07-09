#pragma once
#include "../Util/CHashKey.h"


namespace core::configkeys
{
	/******************************************************************************************************************/
	// Configuration constants:
	/******************************************************************************************************************/
	constexpr char const* k_appDirName						= "SaberEngine\\";
	constexpr char const* k_configDirName					= "Config\\";
	constexpr char const* k_configFileName					= "config.cfg";
	constexpr char const* k_effectDirName					= "Assets\\Effects\\";
	constexpr char const* k_effectManifestFilename			= "EffectManifest.json";
	constexpr char const* k_defaultEngineIBLFilePath		= "Assets\\DefaultIBL\\default.hdr";
	constexpr char const* k_perFileDefaultIBLRelFilePath	= "IBL\\default.hdr"; // Overrides the engine default HDR/IBL
	constexpr char const* k_glslShaderDirName				= "Assets\\Shaders\\GLSL\\";
	constexpr char const* k_hlslShaderDirName				= "Assets\\Shaders\\HLSL\\";
	constexpr char const* k_commonShaderDirName				= "Assets\\Shaders\\Common\\";
	constexpr char const* k_generatedGLSLShaderDirName		= "Assets\\Shaders\\_generated\\GLSL\\"; // Droid only

	// Graphics pipelines:
	constexpr char const* k_pipelineDirName					= "Assets\\Pipelines\\";
	constexpr char const* k_platformPipelineFileName_DX12	= "Platform_DX12.json";
	constexpr char const* k_defaultRenderPipelineFileName	= "DeferredRasterization.json";

	// Debug:
	constexpr char const* k_pixCaptureFolderName		= "PIX Captures";
	constexpr char const* k_renderDocCaptureFolderName	= "RenderDoc Captures";
	constexpr char const* k_captureTitle				= "SaberEngine";

	// ImGui:
	constexpr char const* k_imguiIniPath = "config\\imgui.ini";

	// Logging:
	constexpr char const* k_logFileName		= "SaberEngine.log";
	constexpr char const* k_logOutputDir	= ".\\Logs\\";

	// JSON parsing:
	constexpr util::CHashKey k_jsonAllowExceptionsKey	= "JSONAllowExceptions";
	constexpr util::CHashKey k_jsonIgnoreCommentsKey	= "JSONIgnoreComments"; // Allow C-style comments (not to JSON spec)


	// Command line controls:
	/******************************************************************************************************************/
	constexpr char const* k_importCmdLineArg						= "import";
	constexpr char const* k_showSystemConsoleWindowCmdLineArg		= "console";
	constexpr char const* k_scenePipelineCmdLineArg					= "scenepipeline";
	constexpr char const* k_singleThreadEffectLoading				= "singlethreadeffectloading";
	constexpr char const* k_singleThreadGSExecution					= "singlethreadgsexecution";
	constexpr char const* k_singleThreadGPUResourceCreation			= "singlethreadgpuresourcecreation";
	constexpr char const* k_singleThreadCmdListRecording			= "singlethreadcommandlistrecording";
	constexpr char const* k_singleThreadIndexedBufferUpdates		= "singlethreadindexedbufferupdates";
	
	constexpr char const* k_numWorkerThreads						= "numworkerthreads";
	constexpr char const* k_platformCmdLineArg						= "platform";
	constexpr char const* k_debugLevelCmdLineArg					= "debuglevel";
	constexpr char const* k_enableDredCmdLineArg					= "enabledred";
	constexpr char const* k_enableAftermathCmdLineArg				= "aftermath";
	constexpr char const* k_pixGPUProgrammaticCapturesCmdLineArg	= "pixgpucapture";
	constexpr char const* k_pixCPUProgrammaticCapturesCmdLineArg	= "pixcpucapture";
	constexpr char const* k_renderDocProgrammaticCapturesCmdLineArg	= "renderdoc";
	constexpr char const* k_strictShaderBindingCmdLineArg			= "strictshaderbinding";
	constexpr char const* k_disableCullingCmdLineArg				= "disableculling";


	// Config keys:
	/******************************************************************************************************************/

	// OS:
	constexpr util::CHashKey k_documentsFolderPathKey = "documentsFolderPath"; // e.g. "C:\Users\<username>\Documents"

	// Command line args:
	constexpr util::CHashKey k_commandLineArgsValueKey = "commandLineArgs"; // Get the entire command line arg string

	// Dynamic engine defaults:
	constexpr util::CHashKey k_renderingAPIKey		= "renderingapi";
	constexpr util::CHashKey k_shaderDirectoryKey	= "shaderDirectory"; // e.g. ".\\Shaders\\HLSL\\"

	// System:
	constexpr util::CHashKey k_windowTitleKey		= "windowTitle";
	constexpr util::CHashKey k_windowWidthKey		= "width";
	constexpr util::CHashKey k_windowHeightKey		= "height";
	constexpr util::CHashKey k_numBackbuffersKey	= "numframesinflight";
	constexpr util::CHashKey k_vsyncEnabledKey		= "vsync";

	// Control defaults:
	constexpr util::CHashKey k_mousePitchSensitivityKey	= "mousePitchSensitivity";
	constexpr util::CHashKey k_mouseYawSensitivityKey	= "mouseYawSensitivity";
	constexpr util::CHashKey k_sprintSpeedModifierKey	= "sprintSpeedModifier";

	// Lights/shadows:
	constexpr util::CHashKey k_defaultDirectionalLightMinShadowBiasKey	= "defaultDirectionalLightMinShadowBias";
	constexpr util::CHashKey k_defaultDirectionalLightMaxShadowBiasKey	= "defaultDirectionalLightMaxShadowBias";
	constexpr util::CHashKey k_defaultDirectionalLightShadowSoftnessKey	= "defaultDirectionalLightShadowSoftness";
	constexpr util::CHashKey k_defaultPointLightMinShadowBiasKey		= "defaultPointLightMinShadowBias";
	constexpr util::CHashKey k_defaultPointLightMaxShadowBiasKey		= "defaultPointLightMaxShadowBias";
	constexpr util::CHashKey k_defaultPointLightShadowSoftnessKey		= "defaultPointLightMaxShadowSoftness";
	constexpr util::CHashKey k_defaultSpotLightMinShadowBiasKey			= "defaultSpotLightMinShadowBias";
	constexpr util::CHashKey k_defaultSpotLightMaxShadowBiasKey			= "defaultSpotLightMaxShadowBias";
	constexpr util::CHashKey k_defaultSpotLightShadowSoftnessKey		= "defaultSpotLightMaxShadowSoftness";

	// Camera:
	constexpr util::CHashKey k_defaultFOVKey		= "defaultyCameraFOV";
	constexpr util::CHashKey k_defaultNearKey		= "defaultCameraNear";
	constexpr util::CHashKey k_defaultFarKey		= "defaultCameraFar";

	// Quality:
	constexpr util::CHashKey k_brdfLUTWidthHeightKey					= "brdfLUTWidthHeight";
	constexpr util::CHashKey k_iemTexWidthHeightKey						= "iemWidthHeight";
	constexpr util::CHashKey k_iemNumSamplesKey							= "iemNumSamples";
	constexpr util::CHashKey k_pmremTexWidthHeightKey					= "pmremWidthHeight";
	constexpr util::CHashKey k_pmremNumSamplesKey						= "pmremNumSamples";
	constexpr util::CHashKey k_defaultDirectionalShadowMapResolutionKey	= "defaultDirectionalShadowMapRes";
	constexpr util::CHashKey k_defaultShadowCubeMapResolutionKey		= "defaultShadowCubeMapRes";
	constexpr util::CHashKey k_defaultSpotShadowMapResolutionKey		= "defaultSpotShadowMapRes";

	// Data processing:
	constexpr util::CHashKey k_doCPUVertexStreamNormalizationKey = "cpunormalizevertexstreams";
}


namespace en::DefaultResourceNames
{
	// Engine default resources:
	constexpr char const* k_defaultAlbedoTexName			= "DefaultAlbedoTexture";
	constexpr char const* k_defaultMetallicRoughnessTexName	= "DefaultMetallicRoughnessTexture";
	constexpr char const* k_defaultNormalTexName			= "DefaultNormalTexture";
	constexpr char const* k_defaultOcclusionTexName			= "DefaultOcclusionTexture";
	constexpr char const* k_defaultEmissiveTexName			= "DefaultEmissiveTexture";

	constexpr char const* k_defaultGLTFMaterialName	= "DefaultGLTFMaterial";

	constexpr char const* k_opaqueWhiteDefaultTexName		= "Default2D_OpaqueWhite";
	constexpr char const* k_transparentWhiteDefaultTexName	= "Default2D_TransparentWhite";
	constexpr char const* k_opaqueBlackDefaultTexName		= "Default2D_OpaqueBlack";
	constexpr char const* k_transparentBlackDefaultTexName	= "Default2D_TransparentBlack";
	
	constexpr char const* k_cubeMapOpaqueWhiteDefaultTexName		= "DefaultCube_OpaqueWhite";
	constexpr char const* k_cubeMapTransparentWhiteDefaultTexName	= "DefaultCube_TransparentWhite";
	constexpr char const* k_cubeMapOpaqueBlackDefaultTexName		= "DefaultCube_OpaqueBlack";
	constexpr char const* k_cubeMapTransparentBlackDefaultTexName	= "DefaultCube_TransparentBlack";
}


namespace logging
{
	constexpr char const* k_logPrefix = "Log:\t";
	constexpr wchar_t const* k_logWPrefix = L"Log:\t";
	constexpr size_t k_logPrefixLen = std::char_traits<char>::length(k_logPrefix);

	constexpr char const* k_warnPrefix = "Warn:\t";
	constexpr wchar_t const* k_warnWPrefix = L"Warn:\t";
	constexpr size_t k_warnPrefixLen = std::char_traits<char>::length(k_warnPrefix);

	constexpr char const* k_errorPrefix = "Error:\t";
	constexpr wchar_t const* k_errorWPrefix = L"Error:\t";
	constexpr size_t k_errorPrefixLen = std::char_traits<char>::length(k_errorPrefix);

	constexpr char const* k_newlinePrefix = "\n";
	constexpr wchar_t const* k_newlineWPrefix = L"\n";
	constexpr size_t k_newlinePrefixLen = std::char_traits<char>::length(k_newlinePrefix);

	constexpr char const* k_tabPrefix = "\t";
	constexpr wchar_t const* k_tabWPrefix = L"\t";
	constexpr size_t k_tabPrefixLen = std::char_traits<char>::length(k_tabPrefix);
}