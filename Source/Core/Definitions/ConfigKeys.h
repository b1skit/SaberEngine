#pragma once


namespace core::configkeys
{
	// TODO: Keys should be pre-computed hashes instead of C-strings

	/******************************************************************************************************************/
	// Configuration constants:
	/******************************************************************************************************************/
	constexpr char const* k_configDirName					= "Config\\";
	constexpr char const* k_configFileName					= "config.cfg";
	constexpr char const* k_pipelineDirName					= "Assets\\Pipelines\\";
	constexpr char const* k_defaultScenePipelineFileName	= "scene.json";
	constexpr char const* k_platformPipelineFileName_DX12	= "platformDX12.json";
	constexpr char const* k_glslShaderDirName				= ".\\Shaders\\GLSL\\";
	constexpr char const* k_hlslShaderDirName				= ".\\Shaders\\HLSL\\";
	constexpr char const* k_commonShaderDirName				= ".\\Shaders\\Common\\";

	// Debug:
	constexpr char const* k_pixCaptureFolderName		= "PIX Captures";
	constexpr char const* k_renderDocCaptureFolderName	= "RenderDoc Captures";
	constexpr char const* k_captureTitle				= "SaberEngine";

	// ImGui:
	constexpr char const* k_imguiIniPath = "config\\imgui.ini";

	// Logging:
	constexpr char const* k_logFileName		= "SaberEngine.log";
	constexpr char const* k_logOutputDir	= ".\\Logs\\";


	// Command line controls:
	/******************************************************************************************************************/
	constexpr char const* k_sceneCmdLineArg							= "scene";
	constexpr char const* k_showSystemConsoleWindowCmdLineArg		= "console";
	constexpr char const* k_scenePipelineCmdLineArg					= "scenepipeline";
	constexpr char const* k_singleThreadGSExecution					= "singlethreadgsexecution";
	constexpr char const* k_platformCmdLineArg						= "platform";
	constexpr char const* k_debugLevelCmdLineArg					= "debuglevel";
	constexpr char const* k_enableDredCmdLineArg					= "enabledred";
	constexpr char const* k_pixGPUProgrammaticCapturesCmdLineArg	= "pixgpucapture";
	constexpr char const* k_pixCPUProgrammaticCapturesCmdLineArg	= "pixcpucapture";
	constexpr char const* k_renderDocProgrammaticCapturesCmdLineArg = "renderdoc";
	constexpr char const* k_strictShaderBindingCmdLineArg			= "strictshaderbinding";


	// Config keys:
	/******************************************************************************************************************/

	// OS:
	constexpr char const* k_documentsFolderPathKey = "documentsFolderPath"; // e.g. "C:\Users\<username>\Documents"

	// Command line args:
	constexpr char const* k_commandLineArgsValueKey = "commandLineArgs"; // Gets the command line arg string

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
	constexpr char const* k_mousePitchSensitivityKey	= "mousePitchSensitivity";
	constexpr char const* k_mouseYawSensitivityKey		= "mouseYawSensitivity";
	constexpr char const* k_sprintSpeedModifierKey		= "sprintSpeedModifier";

	// Lights/shadows:
	constexpr char const* k_defaultDirectionalLightMinShadowBiasKey		= "defaultDirectionalLightMinShadowBias";
	constexpr char const* k_defaultDirectionalLightMaxShadowBiasKey		= "defaultDirectionalLightMaxShadowBias";
	constexpr char const* k_defaultDirectionalLightShadowSoftnessKey	= "defaultDirectionalLightShadowSoftness";
	constexpr char const* k_defaultPointLightMinShadowBiasKey			= "defaultPointLightMinShadowBias";
	constexpr char const* k_defaultPointLightMaxShadowBiasKey			= "defaultPointLightMaxShadowBias";
	constexpr char const* k_defaultPointLightShadowSoftnessKey			= "defaultPointLightMaxShadowSoftness";
	constexpr char const* k_defaultSpotLightMinShadowBiasKey			= "defaultSpotLightMinShadowBias";
	constexpr char const* k_defaultSpotLightMaxShadowBiasKey			= "defaultSpotLightMaxShadowBias";
	constexpr char const* k_defaultSpotLightShadowSoftnessKey			= "defaultSpotLightMaxShadowSoftness";

	// Quality:
	constexpr char const* k_brdfLUTWidthHeightKey						= "brdfLUTWidthHeight";
	constexpr char const* k_iemTexWidthHeightKey						= "iemWidthHeight";
	constexpr char const* k_iemNumSamplesKey							= "iemNumSamples";
	constexpr char const* k_pmremTexWidthHeightKey						= "pmremWidthHeight";
	constexpr char const* k_pmremNumSamplesKey							= "pmremNumSamples";
	constexpr char const* k_defaultDirectionalShadowMapResolutionKey	= "defaultDirectionalShadowMapRes";
	constexpr char const* k_defaultShadowCubeMapResolutionKey			= "defaultShadowCubeMapRes";
	constexpr char const* k_defaultSpotShadowMapResolutionKey			= "defaultSpotShadowMapRes";

	// Data processing:
	constexpr char const* k_doCPUVertexStreamNormalizationKey = "cpunormalizevertexstreams";
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

namespace en::DefaultResourceNames
{
	// Engine default resources:
	constexpr char const* k_missingAlbedoTexName			= "MissingAlbedoTexture";
	constexpr char const* k_missingMetallicRoughnessTexName	= "MissingMetallicRoughnessTexture";
	constexpr char const* k_missingNormalTexName			= "MissingNormalTexture";
	constexpr char const* k_missingOcclusionTexName			= "MissingOcclusionTexture";
	constexpr char const* k_missingEmissiveTexName			= "MissingEmissiveTexture";

	constexpr char const* k_missingMaterialName = "MissingMaterial";

	constexpr char const* k_opaqueWhiteDefaultTexName		= "OpaqueWhite";
	constexpr char const* k_transparentWhiteDefaultTexName	= "TransparentWhite";
	constexpr char const* k_opaqueBlackDefaultTexName		= "OpaqueBlack";
	constexpr char const* k_transparentBlackDefaultTexName	= "TransparentBlack";
}