#pragma once


namespace en::ConfigKeys
{
	// TODO: Key names should have "key" post-fixed, and be pre-computed hash keys instead of C-strings

	// Configuration constants:									// From: "Scene\Folder\Names\sceneFile.extension"
	/**************************/
	constexpr char const* k_configDirName	= "Config\\";
	constexpr char const* k_configFileName	= "config.cfg";

	// OS:
	constexpr char const* k_documentsFolderPathKey = "documentsFolderPath"; // e.g. "C:\Users\<username>\Documents"

	// Debug:
	constexpr char const* k_pixCaptureFolderName		= "PIX Captures";
	constexpr char const* k_renderDocCaptureFolderName	= "RenderDoc Captures";
	constexpr char const* k_captureTitle				= "SaberEngine";

	// Logging:
	constexpr char const* k_logFileName		= "SaberEngine.log";
	constexpr char const* k_logOutputDir	= ".\\Logs\\";

	// Config keys:
	/*************/

	// Command line args:
	constexpr char const* k_commandLineArgsValueKey = "commandLineArgs"; // Gets the command line arg string

	constexpr char const* k_sceneCmdLineArg							= "scene";
	constexpr char const* k_showSystemConsoleWindowCmdLineArg		= "console";
	constexpr char const* k_platformCmdLineArg						= "platform";
	constexpr char const* k_debugLevelCmdLineArg					= "debuglevel";
	constexpr char const* k_enableDredCmdLineArg					= "enabledred";
	constexpr char const* k_pixGPUProgrammaticCapturesCmdLineArg	= "pixgpucapture";
	constexpr char const* k_pixCPUProgrammaticCapturesCmdLineArg	= "pixcpucapture";
	constexpr char const* k_renderDocProgrammaticCapturesCmdLineArg = "renderdoc";
	constexpr char const* k_strictShaderBindingCmdLineArg			= "strictshaderbinding";

	// Scene
	constexpr char const* k_scenesDirNameKey	= "ScenesDirname";
	constexpr char const* k_sceneFilePathKey	= "sceneFilePath";	// "Scenes\Scene\Folder\Names\sceneFile.extension"
	constexpr char const* k_sceneNameKey		= "sceneName";		// "sceneFile"
	constexpr char const* k_sceneRootPathKey	= "sceneRootPath";	// ".\Scenes\Scene\Folder\Names\"
	
	constexpr char const* k_sceneIBLDirKey			= "sceneIBLDir";	// "Scenes\SceneFolderName\IBL\"
	constexpr char const* k_sceneIBLPathKey			= "sceneIBLPath";	// "Scenes\SceneFolderName\IBL\ibl.hdr"
	constexpr char const* k_defaultEngineIBLPathKey	= "defaultEngineIBLPath"; // "Assets\\DefaultIBL\\default.hdr"

	constexpr char const* k_shaderDirectoryKey = "shaderDirectory"; // e.g. ".\\Shaders\\HLSL\\"

	// System:
	constexpr char const* k_windowWidthKey		= "width";
	constexpr char const* k_windowHeightKey		= "height";
	constexpr char const* k_numBackbuffersKey	= "numframesinflight"; // DX12 only

	// Control defaults:
	constexpr char const* k_mousePitchSensitivity	= "mousePitchSensitivity";
	constexpr char const* k_mouseYawSensitivity		= "mouseYawSensitivity";
	constexpr char const* k_sprintSpeedModifier		= "sprintSpeedModifier";

	// Lights/shadows:
	constexpr char const* k_defaultDirectionalLightMinShadowBias	= "defaultDirectionalLightMinShadowBias";
	constexpr char const* k_defaultDirectionalLightMaxShadowBias	= "defaultDirectionalLightMaxShadowBias";
	constexpr char const* k_defaultDirectionalLightShadowSoftness	= "defaultDirectionalLightShadowSoftness";
	constexpr char const* k_defaultPointLightMinShadowBias			= "defaultPointLightMinShadowBias";
	constexpr char const* k_defaultPointLightMaxShadowBias			= "defaultPointLightMaxShadowBias";
	constexpr char const* k_defaultPointLightShadowSoftness			= "defaultPointLightMaxShadowSoftness";
	constexpr char const* k_defaultSpotLightMinShadowBias			= "defaultSpotLightMinShadowBias";
	constexpr char const* k_defaultSpotLightMaxShadowBias			= "defaultSpotLightMaxShadowBias";
	constexpr char const* k_defaultSpotLightShadowSoftness			= "defaultSpotLightMaxShadowSoftness";

	// Quality:
	constexpr char const* k_brdfLUTWidthHeight						= "brdfLUTWidthHeight";
	constexpr char const* k_iemTexWidthHeight						= "iemWidthHeight";
	constexpr char const* k_iemNumSamples							= "iemNumSamples";
	constexpr char const* k_pmremTexWidthHeight						= "pmremWidthHeight";
	constexpr char const* k_pmremNumSamples							= "pmremNumSamples";
	constexpr char const* k_defaultDirectionalShadowMapResolution	= "defaultDirectionalShadowMapRes";
	constexpr char const* k_defaultShadowCubeMapResolution			= "defaultShadowCubeMapRes";
	constexpr char const* k_defaultSpotShadowMapResolution			= "defaultSpotShadowMapRes";

	// Data processing:
	constexpr char const* k_doCPUVertexStreamNormalization = "cpunormalizevertexstreams";

	// ImGui:
	constexpr char const* k_imguiIniPath = "config\\imgui.ini";
}

namespace en::ShaderNames
{
	// Shader filename prefixes. Note: These are not config keys
	// TODO: Load these from a file at runtime
	constexpr char const* k_blitShaderName = "Blit";
	constexpr char const* k_bloomShaderName = "Bloom";
	constexpr char const* k_cubeDepthShaderName = "CubeDepth";
	constexpr char const* k_deferredAmbientLightShaderName = "DeferredAmbientLight";
	constexpr char const* k_deferredDirectionalLightShaderName = "DeferredDirectionalLight";
	constexpr char const* k_deferredPointLightShaderName = "DeferredPointLight";
	constexpr char const* k_deferredSpotLightShaderName = "DeferredSpotLight";
	constexpr char const* k_depthShaderName = "Depth";
	constexpr char const* k_gaussianBlurShaderName = "GaussianBlur";
	constexpr char const* k_gbufferShaderName = "GBuffer";
	constexpr char const* k_generateBRDFIntegrationMapShaderName = "GenerateBRDFIntegrationMap";	
	constexpr char const* k_generateIEMShaderName = "GenerateIEM";
	constexpr char const* k_generateMipMapsShaderName = "GenerateMipMaps_BoxFilter";
	constexpr char const* k_generatePMREMShaderName = "GeneratePMREM";
	constexpr char const* k_lineShaderName = "Line";
	constexpr char const* k_skyboxShaderName = "Skybox";
	constexpr char const* k_toneMapShaderName = "ToneMap";
}