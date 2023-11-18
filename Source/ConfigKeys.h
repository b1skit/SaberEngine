#pragma once


namespace en::ConfigKeys
{
	// TODO: Key names should have "key" post-fixed, and be pre-computed hash keys instead of C-strings

	// Configuration constants:
	constexpr char const* k_scenesDirName = "Scenes\\";
	constexpr char const* k_configDirName = "Config\\";
	constexpr char const* k_configFileName = "config.cfg";

	// OS:
	constexpr char const* k_documentsFolderPathKey = "documentsFolderPath"; // e.g. "C:\Users\<username>\Documents"
	constexpr char const* k_pixCaptureFolderName = "PIX Captures";
	constexpr char const* k_renderDocCaptureFolderName = "RenderDoc Captures";
	constexpr char const* k_captureTitle = "SaberEngine";

	// Config keys:
	/*************/
	// Command line args:
	constexpr char const* k_sceneCmdLineArg							= "scene";
	constexpr char const* k_showSystemConsoleWindowCmdLineArg		= "console";
	constexpr char const* k_platformCmdLineArg						= "platform";
	constexpr char const* k_debugLevelCmdLineArg					= "debuglevel";
	constexpr char const* k_pixGPUProgrammaticCapturesCmdLineArg	= "pixgpucapture";
	constexpr char const* k_pixCPUProgrammaticCapturesCmdLineArg	= "pixcpucapture";
	constexpr char const* k_renderDocProgrammaticCapturesCmdLineArg = "renderdoc";
	constexpr char const* k_strictShaderBindingCmdLineArg			= "strictshaderbinding";

	constexpr char const* k_commandLineArgsValueName			= "commandLineArgs"; // Gets the command line arg string

	// System:
	constexpr char const* k_sceneNameValueName		= "sceneName";
	constexpr char const* k_sceneFilePathValueName	= "sceneFilePath";
	constexpr char const* k_windowXResValueName		= "windowXRes";
	constexpr char const* k_windowYResValueName		= "windowYRes";

	// Lights/shadows:
	constexpr char const* k_defaultDirectionalLightMinShadowBias	= "defaultDirectionalLightMinShadowBias";
	constexpr char const* k_defaultDirectionalLightMaxShadowBias	= "defaultDirectionalLightMaxShadowBias";
	constexpr char const* k_defaultPointLightMinShadowBias			= "defaultPointLightMinShadowBias";
	constexpr char const* k_defaultPointLightMaxShadowBias			= "defaultPointLightMaxShadowBias";

	// Quality:
	constexpr char const* k_brdfLUTWidthHeight	= "brdfLUTWidthHeight";
	constexpr char const* k_iemTexWidthHeight	= "iemWidthHeight";
	constexpr char const* k_iemNumSamples		= "iemNumSamples";
	constexpr char const* k_pmremTexWidthHeight = "pmremWidthHeight";
	constexpr char const* k_pmremNumSamples		= "pmremNumSamples";

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