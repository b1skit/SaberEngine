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
	constexpr char const* key_buffers = "Buffers";

	// "DrawStyles":
	constexpr char const* key_conditions = "Conditions";
	constexpr char const* key_rule = "Rule";
	constexpr char const* key_mode = "Mode";
	constexpr char const* key_technique = "Technique";

	// "PipelineStates":
	constexpr char const* key_pipelineStatesBlock = "PipelineStates";
	constexpr char const* key_topologyType = "TopologyType";

	// "RasterizerState":
	constexpr char const* key_rasterizerState = "RasterizerState";
		constexpr char const* key_fillMode = "FillMode";
		constexpr char const* key_faceCullingMode = "FaceCullingMode";
		constexpr char const* key_windingOrder = "WindingOrder";
		constexpr char const* key_depthBias = "DepthBias";
		constexpr char const* key_depthBiasClamp = "DepthBiasClamp";
		constexpr char const* key_slopeScaledDepthBias = "SlopeScaledDepthBias";
		constexpr char const* key_depthClipEnable = "DepthClipEnable";
		constexpr char const* key_multisampleEnable = "MultisampleEnable";
		constexpr char const* key_antialiasedLineEnable = "AntialiasedLineEnable";
		constexpr char const* key_forcedSampleCount = "ForcedSampleCount";
		constexpr char const* key_conservativeRaster = "ConservativeRaster";

	// "DepthStencilState":
	constexpr char const* key_depthStencilState = "DepthStencilState";
		constexpr char const* key_depthTestEnabled = "DepthTestEnabled";
		constexpr char const* key_depthWriteMask = "DepthWriteMask";
		constexpr char const* key_depthComparison = "DepthComparison";
		constexpr char const* key_stencilEnabled = "StencilEnabled";
		constexpr char const* key_stencilReadMask = "StencilReadMask";
		constexpr char const* key_stencilWriteMask = "StencilWriteMask";
		constexpr char const* key_frontStencilOpDesc = "FrontStencilOpDesc";
		constexpr char const* key_backStencilOpDesc = "BackStencilOpDesc";
		constexpr char const* key_stencilFailOp = "StencilFailOp";
		constexpr char const* key_stencilDepthFailOp = "StencilDepthFailOp";
		constexpr char const* key_stencilPassOp = "StencilPassOp";
		constexpr char const* key_stencilComparison = "StencilComparison";

	// "BlendState":
	constexpr char const* key_blendState = "BlendState";
		constexpr char const* key_alphaToCoverageEnable = "AlphaToCoverageEnable";
		constexpr char const* key_independentBlendEnable = "IndependentBlendEnable";
		constexpr char const* key_renderTargets = "RenderTargets";
			constexpr char const* key_blendEnable = "BlendEnable";
			constexpr char const* key_logicOpEnable = "LogicOpEnable";
			constexpr char const* key_srcBlend = "SrcBlend";
			constexpr char const* key_dstBlend = "DstBlend";
			constexpr char const* key_blendOp = "BlendOp";
			constexpr char const* key_srcBlendAlpha = "SrcBlendAlpha";
			constexpr char const* key_dstBlendAlpha = "DstBlendAlpha";
			constexpr char const* key_blendOpAlpha = "BlendOpAlpha";
			constexpr char const* key_logicOp = "LogicOp";
			constexpr char const* key_renderTargetWriteMask = "RenderTargetWriteMask";

	// "VertexStreams"
	constexpr char const* key_vertexStreams = "VertexStreams";
	constexpr char const* key_slots = "Slots";
	constexpr char const* key_dataType = "DataType";
	constexpr char const* key_semantic = "Semantic";

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