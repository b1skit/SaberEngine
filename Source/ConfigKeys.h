#pragma once


namespace en::ConfigKeys
{
	// Configuration constants:
	constexpr char const* k_scenesDirName = "Scenes\\";


	// Config keys:
	constexpr char const* k_sceneCmdLineArg						= "scene";
	constexpr char const* k_showSystemConsoleWindowCmdLineArg	= "console";
	constexpr char const* k_platformCmdLineArg					= "platform";
	constexpr char const* k_debugLevelCmdLineArg				= "debuglevel";
	constexpr char const* k_relaxedShaderBindingCmdLineArg		= "relaxedshaderbinding";

	constexpr char const* k_commandLineArgsValueName			= "commandLineArgs"; // Gets the command line arg string

	constexpr char const* k_sceneNameValueName					= "sceneName";
	constexpr char const* k_sceneFilePathValueName				= "sceneFilePath";
	constexpr char const* k_windowXResValueName					= "windowXRes";
	constexpr char const* k_windowYResValueName					= "windowYRes";
}