// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Shader.h"


namespace
{
	// Effect Manifest:
	//-----------------
	constexpr char const* key_effectsBlock = "Effects";


	// Effect definitions:
	//--------------------

	// Common:
	constexpr char const* key_name = "Name";
	constexpr char const* key_excludedPlatforms = "ExcludedPlatforms";

	// "Effect":
	constexpr char const* key_effectBlock = "Effect";
	constexpr char const* key_parents = "Parents";
	constexpr char const* key_defaultTechnique = "DefaultTechnique";
	constexpr char const* key_drawStyles = "DrawStyles";

	// "DrawStyles":
	constexpr char const* key_conditions = "Conditions";
	constexpr char const* key_rule = "Rule";
	constexpr char const* key_mode = "Mode";
	constexpr char const* key_technique = "Technique";

	// "PipelineStates":
	constexpr char const* key_pipelineStatesBlock = "PipelineStates";
	constexpr char const* key_topologyType = "TopologyType";
	constexpr char const* key_fillMode = "FillMode";
	constexpr char const* key_faceCullingMode = "FaceCullingMode";
	constexpr char const* key_windingOrder = "WindingOrder";
	constexpr char const* key_depthTestMode = "DepthTestMode";

	// "VertexStreams"
	constexpr char const* key_vertexStreams = "VertexStreams";
	constexpr char const* key_slots = "Slots";
	constexpr char const* key_dataType = "DataType";
	constexpr char const* key_semantic = "Semantic";
	constexpr char const* key_morphTargets = "MorphTargets";

	// "Techniques":
	constexpr char const* key_techniques = "Techniques";
	constexpr char const* key_parent = "Parent";
	constexpr char const* key_pipelineState = "PipelineState";
	constexpr char const* key_vertexStream = "VertexStream";
	

	constexpr char const* keys_shaderDefines[] =
	{
		"VShaderDefines",
		"GShaderDefines",
		"PShaderDefines",
		"HShaderDefines",
		"DShaderDefines",
		"MShaderDefines",
		"AShaderDefines",
		"CShaderDefines",
	};

	constexpr char const* keys_shaderTypes[] =
	{
		"VShader",
		"GShader",
		"PShader",
		"HShader",
		"DShader",
		"MShader",
		"AShader",
		"CShader",
	};
	SEStaticAssert(_countof(keys_shaderTypes) == re::Shader::ShaderType_Count, "keys_shaderTypes is out of sync");

	constexpr char const* keys_entryPointNames[] =
	{
		"VShaderEntryPoint",
		"GShaderEntryPoint",
		"PShaderEntryPoint",
		"HShaderEntryPoint",
		"DShaderEntryPoint",
		"MShaderEntryPoint",
		"AShaderEntryPoint",
		"CShaderEntryPoint",
	};
	SEStaticAssert(_countof(keys_entryPointNames) == re::Shader::ShaderType_Count, "keys_entryPointNames is out of sync");
}