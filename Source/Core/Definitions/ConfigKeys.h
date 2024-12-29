#pragma once
#include "../Util/HashKey.h"


namespace core::configkeys
{
	/******************************************************************************************************************/
	// Configuration constants:
	/******************************************************************************************************************/
	constexpr char const* k_appDirName						= "SaberEngine\\";
	constexpr char const* k_configDirName					= "Config\\";
	constexpr char const* k_configFileName					= "config.cfg";
	constexpr char const* k_pipelineDirName					= "Assets\\Pipelines\\";
	constexpr char const* k_defaultScenePipelineFileName	= "scene.json";
	constexpr char const* k_platformPipelineFileName_DX12	= "platformDX12.json";
	constexpr char const* k_effectDirName					= "Assets\\Effects\\";
	constexpr char const* k_effectManifestFilename			= "EffectManifest.json";
	constexpr char const* k_glslShaderDirName				= "Assets\\Shaders\\GLSL\\";
	constexpr char const* k_hlslShaderDirName				= "Assets\\Shaders\\HLSL\\";
	constexpr char const* k_commonShaderDirName				= "Assets\\Shaders\\Common\\";
	constexpr char const* k_generatedGLSLShaderDirName		= "Assets\\Shaders\\Generated\\GLSL\\"; // Droid only

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
	constexpr util::HashKey k_jsonAllowExceptionsKey	= "JSONAllowExceptions";
	constexpr util::HashKey k_jsonIgnoreCommentsKey		= "JSONIgnoreComments"; // Allow C-style comments (not to JSON spec)


	// Command line controls:
	/******************************************************************************************************************/
	constexpr char const* k_sceneCmdLineArg							= "scene";
	constexpr char const* k_showSystemConsoleWindowCmdLineArg		= "console";
	constexpr char const* k_scenePipelineCmdLineArg					= "scenepipeline";
	constexpr char const* k_singleThreadEffectLoading				= "singlethreadeffectloading";
	constexpr char const* k_singleThreadGSExecution					= "singlethreadgsexecution";
	constexpr char const* k_minWorkerThreads						= "minworkerthreads";
	constexpr char const* k_platformCmdLineArg						= "platform";
	constexpr char const* k_debugLevelCmdLineArg					= "debuglevel";
	constexpr char const* k_enableDredCmdLineArg					= "enabledred";
	constexpr char const* k_pixGPUProgrammaticCapturesCmdLineArg	= "pixgpucapture";
	constexpr char const* k_pixCPUProgrammaticCapturesCmdLineArg	= "pixcpucapture";
	constexpr char const* k_renderDocProgrammaticCapturesCmdLineArg	= "renderdoc";
	constexpr char const* k_strictShaderBindingCmdLineArg			= "strictshaderbinding";
	constexpr char const* k_disableCullingCmdLineArg				= "disableculling";


	// Config keys:
	/******************************************************************************************************************/

	// OS:
	constexpr util::HashKey k_documentsFolderPathKey = "documentsFolderPath"; // e.g. "C:\Users\<username>\Documents"

	// Command line args:
	constexpr util::HashKey k_commandLineArgsValueKey = "commandLineArgs"; // Gets the command line arg string

	// Scene
	constexpr util::HashKey k_scenesDirNameKey	= "ScenesDirName";
	constexpr util::HashKey k_sceneFilePathKey	= "sceneFilePath";	// "Scenes\Scene\Folder\Names\sceneFile.extension"
	constexpr util::HashKey k_sceneNameKey		= "sceneName";		// "sceneFile"
	constexpr util::HashKey k_sceneRootPathKey	= "sceneRootPath";	// ".\Scenes\Scene\Folder\Names\"
	
	constexpr util::HashKey k_sceneIBLDirKey			= "sceneIBLDir";	// "Scenes\SceneFolderName\IBL\"
	constexpr util::HashKey k_sceneIBLPathKey			= "sceneIBLPath";	// "Scenes\SceneFolderName\IBL\ibl.hdr"
	constexpr util::HashKey k_defaultEngineIBLPathKey	= "defaultEngineIBLPath"; // "Assets\\DefaultIBL\\default.hdr"

	constexpr util::HashKey k_shaderDirectoryKey = "shaderDirectory"; // e.g. ".\\Shaders\\HLSL\\"

	// System:
	constexpr util::HashKey k_windowTitleKey		= "windowTitle";
	constexpr util::HashKey k_windowWidthKey		= "width";
	constexpr util::HashKey k_windowHeightKey		= "height";
	constexpr util::HashKey k_numBackbuffersKey		= "numframesinflight";
	constexpr util::HashKey k_vsyncEnabledKey		= "vsync";

	// Control defaults:
	constexpr util::HashKey k_mousePitchSensitivityKey	= "mousePitchSensitivity";
	constexpr util::HashKey k_mouseYawSensitivityKey	= "mouseYawSensitivity";
	constexpr util::HashKey k_sprintSpeedModifierKey	= "sprintSpeedModifier";

	// Lights/shadows:
	constexpr util::HashKey k_defaultDirectionalLightMinShadowBiasKey	= "defaultDirectionalLightMinShadowBias";
	constexpr util::HashKey k_defaultDirectionalLightMaxShadowBiasKey	= "defaultDirectionalLightMaxShadowBias";
	constexpr util::HashKey k_defaultDirectionalLightShadowSoftnessKey	= "defaultDirectionalLightShadowSoftness";
	constexpr util::HashKey k_defaultPointLightMinShadowBiasKey			= "defaultPointLightMinShadowBias";
	constexpr util::HashKey k_defaultPointLightMaxShadowBiasKey			= "defaultPointLightMaxShadowBias";
	constexpr util::HashKey k_defaultPointLightShadowSoftnessKey		= "defaultPointLightMaxShadowSoftness";
	constexpr util::HashKey k_defaultSpotLightMinShadowBiasKey			= "defaultSpotLightMinShadowBias";
	constexpr util::HashKey k_defaultSpotLightMaxShadowBiasKey			= "defaultSpotLightMaxShadowBias";
	constexpr util::HashKey k_defaultSpotLightShadowSoftnessKey			= "defaultSpotLightMaxShadowSoftness";

	// Camera:
	constexpr util::HashKey k_forceDefaultCameraKey = "forceCreateDefaultCamera";
	constexpr util::HashKey k_defaultFOVKey			= "defaultyCameraFOV";
	constexpr util::HashKey k_defaultNearKey		= "defaultCameraNear";
	constexpr util::HashKey k_defaultFarKey			= "defaultCameraFar";

	// Quality:
	constexpr util::HashKey k_brdfLUTWidthHeightKey						= "brdfLUTWidthHeight";
	constexpr util::HashKey k_iemTexWidthHeightKey						= "iemWidthHeight";
	constexpr util::HashKey k_iemNumSamplesKey							= "iemNumSamples";
	constexpr util::HashKey k_pmremTexWidthHeightKey					= "pmremWidthHeight";
	constexpr util::HashKey k_pmremNumSamplesKey						= "pmremNumSamples";
	constexpr util::HashKey k_defaultDirectionalShadowMapResolutionKey	= "defaultDirectionalShadowMapRes";
	constexpr util::HashKey k_defaultShadowCubeMapResolutionKey			= "defaultShadowCubeMapRes";
	constexpr util::HashKey k_defaultSpotShadowMapResolutionKey			= "defaultSpotShadowMapRes";

	// Data processing:
	constexpr util::HashKey k_doCPUVertexStreamNormalizationKey = "cpunormalizevertexstreams";
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

	constexpr char const* k_defaultIBLTexName = "DefaultIBL"; // Get whatever IBL was loaded as the default
}