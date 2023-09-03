#pragma once


namespace en::ConfigKeys
{
	// Configuration constants:
	constexpr char const* k_scenesDirName = "Scenes\\";


	// Config keys:
	/*************/
	// Command line args:
	constexpr char const* k_sceneCmdLineArg						= "scene";
	constexpr char const* k_showSystemConsoleWindowCmdLineArg	= "console";
	constexpr char const* k_platformCmdLineArg					= "platform";
	constexpr char const* k_debugLevelCmdLineArg				= "debuglevel";
	constexpr char const* k_strictShaderBindingCmdLineArg		= "strictshaderbinding";

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
}

namespace en::ShaderNames
{
	// Shader filename prefixes. Note: These are not config keys
	constexpr char const* k_mipGenerationShaderName = "GenerateMipMaps_BoxFilter";
	constexpr char const* k_gbufferShaderName = "GBuffer";
	constexpr char const* k_generateBRDFIntegrationMapShaderName = "GenerateBRDFIntegrationMap";
}