// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <d3d12shader.h>
#include <d3dcompiler.h>

#include "Context_DX12.h"
#include "DebugConfiguration.h"
#include "Debug_DX12.h"
#include "MeshPrimitive.h"
#include "PipelineState.h"
#include "PipelineState_DX12.h"
#include "Shader.h"
#include "Shader_DX12.h"
#include "Texture_DX12.h"
#include "TextureTarget.h"
#include "TextureTarget_DX12.h"


namespace
{
	using Microsoft::WRL::ComPtr;
	using dx12::CheckHResult;
	
	
	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS vShader;
		CD3DX12_PIPELINE_STATE_STREAM_PS pShader;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER rasterizer;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL depthStencil;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC blend;
	};


	DXGI_FORMAT GetDefaultInputParameterFormat(std::string const& semantic)
	{
		static const std::unordered_map<std::string, DXGI_FORMAT> k_semanticToFormat =
		{
			{"POSITION",		DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT},
			{"NORMAL",			DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT},
			//{"BINORMAL",		DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT},
			{"TANGENT",			DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT},
			{"TEXCOORD",		DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT},
			{"COLOR",			DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT},

			{"BLENDINDICES",	DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UINT},
			{"BLENDWEIGHT",		DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT},
		};
		SEAssert("Missing semantics", k_semanticToFormat.size() == re::MeshPrimitive::Slot_CountNoIndices);

		auto const& result = k_semanticToFormat.find(semantic);
		SEAssert("Invalid semantic name", result != k_semanticToFormat.end());

		return result->second;
	}


	uint32_t GetDefaultInputSlot(std::string const& semantic, uint32_t semanticIndex)
	{
		static const std::unordered_map<std::string, uint32_t> k_sematicToSlot =
		{
			{"POSITION0",		re::MeshPrimitive::Position},
			{"NORMAL0",			re::MeshPrimitive::Normal},
			//{"BINORMAL0",		re::MeshPrimitive::}, //
			{"TANGENT0",		re::MeshPrimitive::Tangent},
			{"TEXCOORD0",		re::MeshPrimitive::UV0},
			{"COLOR0",			re::MeshPrimitive::Color},

			{"BLENDINDICES0",	re::MeshPrimitive::Joints},
			{"BLENDWEIGHT0",	re::MeshPrimitive::Weights},
		};
		SEAssert("Missing semantics", k_sematicToSlot.size() == re::MeshPrimitive::Slot_CountNoIndices);

		const std::string semanticAndIndex = semantic + std::to_string(semanticIndex);

		auto const& result = k_sematicToSlot.find(semanticAndIndex);
		SEAssert("Invalid semantic and/or index", result != k_sematicToSlot.end());

		return result->second;
	}


	void BuildInputLayout(
		dx12::Shader::PlatformParams* shaderParams, std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout)
	{
		ID3D12ShaderReflection* shaderReflection;
		HRESULT hr = ::D3DReflect(
			shaderParams->m_shaderBlobs[dx12::Shader::Vertex]->GetBufferPointer(),
			shaderParams->m_shaderBlobs[dx12::Shader::Vertex]->GetBufferSize(),
			IID_PPV_ARGS(&shaderReflection));
		CheckHResult(hr, "Failed to reflect shader");

		for (uint32_t paramIndex = 0; paramIndex < dx12::Shader::k_maxVShaderVertexInputs; paramIndex++)
		{
			D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
			hr = shaderReflection->GetInputParameterDesc(paramIndex, &paramDesc);

			if (hr != S_OK)
			{
				break;
			}

			// Skip System Value semantics:
			if (strcmp(paramDesc.SemanticName, "SV_InstanceID") == 0)
			{
				continue;
			}

			inputLayout.emplace_back(D3D12_INPUT_ELEMENT_DESC(
				paramDesc.SemanticName,									// Semantic name
				paramDesc.SemanticIndex,								// Semantic idx: Only needed when >1 element of same semantic
				GetDefaultInputParameterFormat(paramDesc.SemanticName),	// Format
				GetDefaultInputSlot(paramDesc.SemanticName, paramDesc.SemanticIndex),	// Input slot [0, 15]
				D3D12_APPEND_ALIGNED_ELEMENT,							// Aligned byte offset
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,				// Input slot class
				0														// Input data step rate
			));
		}
	}


	D3D12_RASTERIZER_DESC BuildRasterizerDesc(gr::PipelineState const& grPipelineState)
	{
		D3D12_RASTERIZER_DESC rasterizerDesc{};

		// Polygon fill mode:
		switch (grPipelineState.GetFillMode())
		{
		case gr::PipelineState::FillMode::Wireframe:
			rasterizerDesc.FillMode = D3D12_FILL_MODE::D3D12_FILL_MODE_WIREFRAME; break;
		case gr::PipelineState::FillMode::Solid:
			rasterizerDesc.FillMode = D3D12_FILL_MODE::D3D12_FILL_MODE_SOLID; break;
		default:
			SEAssertF("Invalid fill mode");
			rasterizerDesc.FillMode = D3D12_FILL_MODE::D3D12_FILL_MODE_SOLID;
		}

		// Face culling mode:
		switch (grPipelineState.GetFaceCullingMode())
		{
		case gr::PipelineState::FaceCullingMode::Disabled:
			rasterizerDesc.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_NONE; break;
		case gr::PipelineState::FaceCullingMode::Front:
			rasterizerDesc.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_FRONT; break;
		case gr::PipelineState::FaceCullingMode::Back:
			rasterizerDesc.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_BACK; break;			
		default:
			SEAssertF("Invalid fill mode");
			rasterizerDesc.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_BACK;
		}

		// Winding order:
		switch (grPipelineState.GetWindingOrder())
		{
		case gr::PipelineState::WindingOrder::CCW:
			rasterizerDesc.FrontCounterClockwise = true; break;
		case gr::PipelineState::WindingOrder::CW:
			rasterizerDesc.FrontCounterClockwise = false; break;
		default:
			SEAssertF("Invalid winding order");
			rasterizerDesc.FrontCounterClockwise = true;
		}
		
		// TODO: Support these via the gr::PipelineState
		rasterizerDesc.DepthBias = 0;
		rasterizerDesc.DepthBiasClamp = 0.f;
		rasterizerDesc.SlopeScaledDepthBias = 0.f;
		rasterizerDesc.DepthClipEnable = true;
		rasterizerDesc.MultisampleEnable = false;
		rasterizerDesc.AntialiasedLineEnable = false; // Only applies if drawing lines with .MultisampleEnable = false
		rasterizerDesc.ForcedSampleCount = 0;
		rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE::D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		return rasterizerDesc;
	}


	D3D12_DEPTH_STENCIL_DESC BuildDepthStencilDesc(gr::PipelineState const& grPipelineState)
	{
		D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

		depthStencilDesc.DepthEnable = true; // TODO: Support toggling this via the gr::PipelineState

		// Depth testing:
		switch (grPipelineState.GetDepthTestMode())
		{
		case gr::PipelineState::DepthTestMode::Default: // Less
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS; break;
		case gr::PipelineState::DepthTestMode::Never: // Never pass
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_NEVER; break;
		case gr::PipelineState::DepthTestMode::Less:
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS; break;
		case gr::PipelineState::DepthTestMode::Equal:
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL; break;
		case gr::PipelineState::DepthTestMode::LEqual:
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; break;
		case gr::PipelineState::DepthTestMode::Greater:
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER; break;
		case gr::PipelineState::DepthTestMode::NotEqual:
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL; break;
		case gr::PipelineState::DepthTestMode::GEqual:
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL; break;
		case gr::PipelineState::DepthTestMode::Always: // Always pass: Disables depth testing
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS; break;
		default:
			SEAssertF("Invalid depth test mode");
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		}

		// Depth write mode:
		switch (grPipelineState.GetDepthWriteMode())
		{
		case gr::PipelineState::DepthWriteMode::Enabled:
			depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; break;
		case gr::PipelineState::DepthWriteMode::Disabled:
			depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; break;
		default:
			SEAssertF("Invalid depth write mode");
			depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		}

		// TODO: Support these via the gr::PipelineState
		depthStencilDesc.StencilEnable = false;
		depthStencilDesc.StencilReadMask = 0;
		depthStencilDesc.StencilWriteMask = 0;

		D3D12_DEPTH_STENCILOP_DESC frontFaceDesc{};
		depthStencilDesc.FrontFace = frontFaceDesc;

		D3D12_DEPTH_STENCILOP_DESC backFaceDesc{};
		depthStencilDesc.BackFace = backFaceDesc;

		return depthStencilDesc;
	}


	D3D12_BLEND_DESC BuildBlendDesc(re::TextureTargetSet const& targetSet, gr::PipelineState const& grPipelineState)
	{
		D3D12_BLEND_DESC blendDesc{};

		// TODO: Support these via the gr::PipelineState
		blendDesc.AlphaToCoverageEnable = false;
		blendDesc.IndependentBlendEnable = false;

		// Configure the target blending:
		D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc{};

		// Source blending:
		switch (grPipelineState.GetSrcBlendMode())
		{
		case gr::PipelineState::BlendMode::Disabled:
		{
			SEAssert("Must disable blending for both source and destination",
				grPipelineState.GetSrcBlendMode() == grPipelineState.GetDstBlendMode());

			rtBlendDesc.BlendEnable = false;
			rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_ZERO;
		}
		break;
		case gr::PipelineState::BlendMode::Default: // Src one, Dst zero
			rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_ONE; break;
		case gr::PipelineState::BlendMode::Zero:
			rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_ZERO; break;
		case gr::PipelineState::BlendMode::One:
			rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_ONE; break;
		case gr::PipelineState::BlendMode::SrcColor:
			rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_SRC_COLOR; break;
		case gr::PipelineState::BlendMode::OneMinusSrcColor:
			rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_INV_SRC_COLOR; break;
		case gr::PipelineState::BlendMode::DstColor:
			rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_DEST_COLOR; break;
		case gr::PipelineState::BlendMode::OneMinusDstColor:
			rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_INV_DEST_COLOR; break;
		case gr::PipelineState::BlendMode::SrcAlpha:
			rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_SRC_ALPHA; break;
		case gr::PipelineState::BlendMode::OneMinusSrcAlpha:
			rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_INV_SRC_ALPHA; break;
		case gr::PipelineState::BlendMode::DstAlpha:
			rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_DEST_ALPHA; break;
		case gr::PipelineState::BlendMode::OneMinusDstAlpha:
			rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_INV_DEST_ALPHA; break;
		default:
			SEAssertF("Invalid source blend mode");
			rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_ONE;
		}

		// Destination blending:
		switch (grPipelineState.GetDstBlendMode())
		{
		case gr::PipelineState::BlendMode::Disabled:
		{
			SEAssert("Must disable blending for both source and destination",
				grPipelineState.GetSrcBlendMode() == grPipelineState.GetDstBlendMode());

			rtBlendDesc.BlendEnable = false;
			rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_ZERO;
		}
		break;
		case gr::PipelineState::BlendMode::Default: // Src one, Dst zero
			rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_ZERO; break;
		case gr::PipelineState::BlendMode::Zero:
			rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_ZERO; break;
		case gr::PipelineState::BlendMode::One:
			rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_ONE; break;
		case gr::PipelineState::BlendMode::SrcColor:
			rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_SRC_COLOR; break;
		case gr::PipelineState::BlendMode::OneMinusSrcColor:
			rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_INV_SRC_COLOR; break;
		case gr::PipelineState::BlendMode::DstColor:
			rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_DEST_COLOR; break;
		case gr::PipelineState::BlendMode::OneMinusDstColor:
			rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_INV_DEST_COLOR; break;
		case gr::PipelineState::BlendMode::SrcAlpha:
			rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_SRC_ALPHA; break;
		case gr::PipelineState::BlendMode::OneMinusSrcAlpha:
			rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_INV_SRC_ALPHA; break;
		case gr::PipelineState::BlendMode::DstAlpha:
			rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_DEST_ALPHA; break;
		case gr::PipelineState::BlendMode::OneMinusDstAlpha:
			rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_INV_DEST_ALPHA; break;
		default:
			SEAssertF("Invalid dest blend mode");
			rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_ZERO;
		}

		// TODO: Support these via the gr::PipelineState
		rtBlendDesc.LogicOpEnable = false;
		rtBlendDesc.BlendOp = D3D12_BLEND_OP::D3D12_BLEND_OP_ADD;
		rtBlendDesc.SrcBlendAlpha = D3D12_BLEND::D3D12_BLEND_ONE;
		rtBlendDesc.DestBlendAlpha = D3D12_BLEND::D3D12_BLEND_ZERO;
		rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP::D3D12_BLEND_OP_ADD;
		rtBlendDesc.LogicOp = D3D12_LOGIC_OP::D3D12_LOGIC_OP_NOOP;

		// Color write modes:
		rtBlendDesc.RenderTargetWriteMask = 
			(grPipelineState.GetColorWriteMode().R == gr::PipelineState::ColorWriteMode::ChannelMode::Enabled ? D3D12_COLOR_WRITE_ENABLE_RED : 0) |
			(grPipelineState.GetColorWriteMode().G == gr::PipelineState::ColorWriteMode::ChannelMode::Enabled ? D3D12_COLOR_WRITE_ENABLE_GREEN : 0) |
			(grPipelineState.GetColorWriteMode().B == gr::PipelineState::ColorWriteMode::ChannelMode::Enabled ? D3D12_COLOR_WRITE_ENABLE_BLUE : 0) |
			(grPipelineState.GetColorWriteMode().A == gr::PipelineState::ColorWriteMode::ChannelMode::Enabled ? D3D12_COLOR_WRITE_ENABLE_ALPHA : 0);

		// TODO: This should be per-target (i.e. for MRT), but currently it's per stage
		blendDesc.RenderTarget[0] = rtBlendDesc;

		return blendDesc;
	}
}


namespace dx12
{
	PipelineState::PipelineState()
		: m_pipelineState(nullptr)
	{
	}


	void PipelineState::Create(
		re::Shader const& shader,
		gr::PipelineState const& grPipelineState,
		re::TextureTargetSet& targetSet)
	{
		m_rootSignature.Create(shader);


		dx12::Shader::PlatformParams* shaderParams = shader.GetPlatformParams()->As<dx12::Shader::PlatformParams*>();

		SEAssert("Shader doesn't have a pixel and vertex shader blob. TODO: Support this",
			shaderParams->m_shaderBlobs[dx12::Shader::Vertex] && shaderParams->m_shaderBlobs[dx12::Shader::Pixel]);

		if (shaderParams->m_shaderBlobs[dx12::Shader::Vertex] != nullptr)
		{
			// Build the vertex stream input layout:
			std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
			BuildInputLayout(shaderParams, inputLayout);

			// Build pipeline descriptions:
			PipelineStateStream pipelineStateStream;
			pipelineStateStream.rootSignature = m_rootSignature.GetD3DRootSignature();
			pipelineStateStream.inputLayout = { &inputLayout[0], static_cast<uint32_t>(inputLayout.size()) };
			pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateStream.vShader = CD3DX12_SHADER_BYTECODE(shaderParams->m_shaderBlobs[dx12::Shader::Vertex].Get());
			pipelineStateStream.pShader = CD3DX12_SHADER_BYTECODE(shaderParams->m_shaderBlobs[dx12::Shader::Pixel].Get());

			// Ensure the targets are created:
			if (!targetSet.GetPlatformParams()->m_colorIsCreated)
			{
				dx12::TextureTargetSet::CreateColorTargets(targetSet);
			}
			if (!targetSet.GetPlatformParams()->m_depthIsCreated)
			{
				dx12::TextureTargetSet::CreateDepthStencilTarget(targetSet);
			}
			// TODO: Should we add TextureTargetSets to the API layer creation queue?
			// TODO: We're currently assuming target sets have both color and depth targets... This is not always true!

			// Target formats:
			dx12::TextureTargetSet::PlatformParams* targetSetPlatParams =
				targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();
			pipelineStateStream.RTVFormats = TextureTargetSet::GetColorTargetFormats(targetSet);
			pipelineStateStream.DSVFormat = 
				targetSet.GetDepthStencilTarget().GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>()->m_format;

			// Rasterizer description:
			const D3D12_RASTERIZER_DESC rasterizerDesc = BuildRasterizerDesc(grPipelineState);
			pipelineStateStream.rasterizer = CD3DX12_RASTERIZER_DESC(rasterizerDesc);

			// Depth stencil description:
			const D3D12_DEPTH_STENCIL_DESC depthStencilDesc = BuildDepthStencilDesc(grPipelineState);
			pipelineStateStream.depthStencil = CD3DX12_DEPTH_STENCIL_DESC(depthStencilDesc);

			// Blend description:
			const D3D12_BLEND_DESC blendDesc = BuildBlendDesc(targetSet, grPipelineState);
			pipelineStateStream.blend = CD3DX12_BLEND_DESC(blendDesc);

			D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc =
			{
				sizeof(PipelineStateStream),
				&pipelineStateStream
			};

			// Generate the PSO:
			dx12::Context::PlatformParams* ctxPlatParams =
				re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();
			ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

			HRESULT hr = device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_pipelineState));
			CheckHResult(hr, "Failed to create pipeline state");
		}
		else
		{
			SEAssertF("Found a Shader object without a vertex shader. TODO: Support this (e.g. compute shaders)");
		}

	}


	void PipelineState::Destroy()
	{
		m_rootSignature.Destroy();
		m_pipelineState = nullptr;		
	}


	ID3D12PipelineState* PipelineState::GetD3DPipelineState() const
	{
		return m_pipelineState.Get();
	}


	dx12::RootSignature const& PipelineState::GetRootSignature() const
	{
		return m_rootSignature;
	}
}