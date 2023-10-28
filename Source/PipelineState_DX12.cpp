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
#include "SysInfo_DX12.h"
#include "Texture_DX12.h"
#include "TextureTarget.h"
#include "TextureTarget_DX12.h"


namespace
{
	using Microsoft::WRL::ComPtr;
	using dx12::CheckHResult;
	
	// TODO: Use D3D12_GRAPHICS_PIPELINE_STATE_DESC instead?
	struct GraphicsPipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS vShader;
		CD3DX12_PIPELINE_STATE_STREAM_GS gShader;
		CD3DX12_PIPELINE_STATE_STREAM_PS pShader;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER rasterizer;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL depthStencil;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC blend;
	};


	struct ComputePipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_CS cShader;
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
		SEAssert("Missing semantics", k_semanticToFormat.size() == gr::MeshPrimitive::Slot_Count);

		auto const& result = k_semanticToFormat.find(semantic);
		SEAssert("Invalid semantic name", result != k_semanticToFormat.end());

		return result->second;
	}


	uint32_t GetDefaultInputSlot(std::string const& semantic, uint32_t semanticIndex)
	{
		static const std::unordered_map<std::string, uint32_t> k_sematicToSlot =
		{
			{"POSITION0",		gr::MeshPrimitive::Position},
			{"NORMAL0",			gr::MeshPrimitive::Normal},
			//{"BINORMAL0",		gr::MeshPrimitive::}, //
			{"TANGENT0",		gr::MeshPrimitive::Tangent},
			{"TEXCOORD0",		gr::MeshPrimitive::UV0},
			{"COLOR0",			gr::MeshPrimitive::Color},

			{"BLENDINDICES0",	gr::MeshPrimitive::Joints},
			{"BLENDWEIGHT0",	gr::MeshPrimitive::Weights},
		};
		SEAssert("Missing semantics", k_sematicToSlot.size() == gr::MeshPrimitive::Slot_Count);

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


	D3D12_RASTERIZER_DESC BuildRasterizerDesc(re::PipelineState const& rePipelineState)
	{
		D3D12_RASTERIZER_DESC rasterizerDesc{};

		// Polygon fill mode:
		switch (rePipelineState.GetFillMode())
		{
		case re::PipelineState::FillMode::Wireframe:
			rasterizerDesc.FillMode = D3D12_FILL_MODE::D3D12_FILL_MODE_WIREFRAME; break;
		case re::PipelineState::FillMode::Solid:
			rasterizerDesc.FillMode = D3D12_FILL_MODE::D3D12_FILL_MODE_SOLID; break;
		default:
			SEAssertF("Invalid fill mode");
			rasterizerDesc.FillMode = D3D12_FILL_MODE::D3D12_FILL_MODE_SOLID;
		}

		// Face culling mode:
		switch (rePipelineState.GetFaceCullingMode())
		{
		case re::PipelineState::FaceCullingMode::Disabled:
			rasterizerDesc.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_NONE; break;
		case re::PipelineState::FaceCullingMode::Front:
			rasterizerDesc.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_FRONT; break;
		case re::PipelineState::FaceCullingMode::Back:
			rasterizerDesc.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_BACK; break;			
		default:
			SEAssertF("Invalid fill mode");
			rasterizerDesc.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_BACK;
		}

		// Winding order:
		switch (rePipelineState.GetWindingOrder())
		{
		case re::PipelineState::WindingOrder::CCW:
			rasterizerDesc.FrontCounterClockwise = true; break;
		case re::PipelineState::WindingOrder::CW:
			rasterizerDesc.FrontCounterClockwise = false; break;
		default:
			SEAssertF("Invalid winding order");
			rasterizerDesc.FrontCounterClockwise = true;
		}
		
		// TODO: Support these via the re::PipelineState
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


	D3D12_DEPTH_STENCIL_DESC BuildDepthStencilDesc(
		re::TextureTarget const* depthTarget, re::PipelineState const& rePipelineState)
	{
		// We make assumptions when recording resource transitions on our command lists that depth targets will 
		// specifically have depth disabled (not just masked out) when the depth channel write mode is disabled
		const bool depthEnabled = depthTarget && depthTarget->HasTexture();
		
		const bool depthWritesEnabled = depthEnabled &&
			depthTarget->GetDepthWriteMode() == re::TextureTarget::TargetParams::ChannelWrite::Mode::Enabled;

		D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

		depthStencilDesc.DepthEnable = depthEnabled;

		depthStencilDesc.DepthWriteMask = depthWritesEnabled ?
			D3D12_DEPTH_WRITE_MASK::D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK::D3D12_DEPTH_WRITE_MASK_ZERO;

		// Depth testing:
		switch (rePipelineState.GetDepthTestMode())
		{
		case re::PipelineState::DepthTestMode::Never: // Never pass
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_NEVER; break;
		case re::PipelineState::DepthTestMode::Less:
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS; break;
		case re::PipelineState::DepthTestMode::Equal:
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL; break;
		case re::PipelineState::DepthTestMode::LEqual:
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; break;
		case re::PipelineState::DepthTestMode::Greater:
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER; break;
		case re::PipelineState::DepthTestMode::NotEqual:
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL; break;
		case re::PipelineState::DepthTestMode::GEqual:
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL; break;
		case re::PipelineState::DepthTestMode::Always: // Always pass: Disables depth testing
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS; break;
		default:
			SEAssertF("Invalid depth test mode");
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		}

		// TODO: Support these
		SEAssert("TODO: Support StencilTarget and DepthStencilTarget usages",
			!depthTarget ||
			!depthTarget->GetTexture() ||
			!((depthTarget->GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::StencilTarget) ||
				(depthTarget->GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::DepthStencilTarget)));
		depthStencilDesc.StencilEnable = false;
		depthStencilDesc.StencilReadMask = 0;
		depthStencilDesc.StencilWriteMask = 0;

		D3D12_DEPTH_STENCILOP_DESC frontFaceDesc{};
		depthStencilDesc.FrontFace = frontFaceDesc;

		D3D12_DEPTH_STENCILOP_DESC backFaceDesc{};
		depthStencilDesc.BackFace = backFaceDesc;

		return depthStencilDesc;
	}


	D3D12_BLEND_DESC BuildBlendDesc(re::TextureTargetSet const& targetSet, re::PipelineState const& rePipelineState)
	{
		D3D12_BLEND_DESC blendDesc{};

		// TODO: Support these
		blendDesc.AlphaToCoverageEnable = false;
		blendDesc.IndependentBlendEnable = false;

		// Configure the blend mode for each target:
		for (uint32_t i = 0; i < dx12::SysInfo::GetMaxRenderTargets(); i++)
		{
			D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc{};
			rtBlendDesc.BlendEnable = true;

			re::TextureTarget::TargetParams::BlendModes const& blendModes = targetSet.GetColorTarget(i).GetBlendMode();

			// Source blending:
			switch (blendModes.m_srcBlendMode)
			{
			case re::TextureTarget::TargetParams::BlendMode::Disabled:
			{
				SEAssert("Must disable blending for both source and destination",
					blendModes.m_srcBlendMode == blendModes.m_dstBlendMode);

				rtBlendDesc.BlendEnable = false;
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_ZERO;
			}
			break;
			case re::TextureTarget::TargetParams::BlendMode::Zero:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_ZERO; break;
			case re::TextureTarget::TargetParams::BlendMode::One:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_ONE; break;
			case re::TextureTarget::TargetParams::BlendMode::SrcColor:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_SRC_COLOR; break;
			case re::TextureTarget::TargetParams::BlendMode::OneMinusSrcColor:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_INV_SRC_COLOR; break;
			case re::TextureTarget::TargetParams::BlendMode::DstColor:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_DEST_COLOR; break;
			case re::TextureTarget::TargetParams::BlendMode::OneMinusDstColor:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_INV_DEST_COLOR; break;
			case re::TextureTarget::TargetParams::BlendMode::SrcAlpha:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_SRC_ALPHA; break;
			case re::TextureTarget::TargetParams::BlendMode::OneMinusSrcAlpha:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_INV_SRC_ALPHA; break;
			case re::TextureTarget::TargetParams::BlendMode::DstAlpha:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_DEST_ALPHA; break;
			case re::TextureTarget::TargetParams::BlendMode::OneMinusDstAlpha:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_INV_DEST_ALPHA; break;
			default:
				SEAssertF("Invalid source blend mode");
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_ONE;
			}

			// Destination blending:
			switch (blendModes.m_dstBlendMode)
			{
			case re::TextureTarget::TargetParams::BlendMode::Disabled:
			{
				SEAssert("Must disable blending for both source and destination",
					blendModes.m_srcBlendMode == blendModes.m_dstBlendMode);

				rtBlendDesc.BlendEnable = false;
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_ZERO;
			}
			break;
			case re::TextureTarget::TargetParams::BlendMode::Zero:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_ZERO; break;
			case re::TextureTarget::TargetParams::BlendMode::One:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_ONE; break;
			case re::TextureTarget::TargetParams::BlendMode::SrcColor:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_SRC_COLOR; break;
			case re::TextureTarget::TargetParams::BlendMode::OneMinusSrcColor:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_INV_SRC_COLOR; break;
			case re::TextureTarget::TargetParams::BlendMode::DstColor:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_DEST_COLOR; break;
			case re::TextureTarget::TargetParams::BlendMode::OneMinusDstColor:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_INV_DEST_COLOR; break;
			case re::TextureTarget::TargetParams::BlendMode::SrcAlpha:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_SRC_ALPHA; break;
			case re::TextureTarget::TargetParams::BlendMode::OneMinusSrcAlpha:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_INV_SRC_ALPHA; break;
			case re::TextureTarget::TargetParams::BlendMode::DstAlpha:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_DEST_ALPHA; break;
			case re::TextureTarget::TargetParams::BlendMode::OneMinusDstAlpha:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_INV_DEST_ALPHA; break;
			default:
				SEAssertF("Invalid dest blend mode");
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_ZERO;
			}

			// TODO: Support these
			rtBlendDesc.LogicOpEnable = false;
			rtBlendDesc.BlendOp = D3D12_BLEND_OP::D3D12_BLEND_OP_ADD;
			rtBlendDesc.SrcBlendAlpha = D3D12_BLEND::D3D12_BLEND_ONE;
			rtBlendDesc.DestBlendAlpha = D3D12_BLEND::D3D12_BLEND_ZERO;
			rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP::D3D12_BLEND_OP_ADD;
			rtBlendDesc.LogicOp = D3D12_LOGIC_OP::D3D12_LOGIC_OP_NOOP;

			// Build a bitmask for our color write modes:
			re::TextureTarget::TargetParams::ChannelWrite const& colorWriteMode =
				targetSet.GetColorTarget(i).GetColorWriteMode();

			rtBlendDesc.RenderTargetWriteMask =
				(colorWriteMode.R == re::TextureTarget::TargetParams::ChannelWrite::Mode::Enabled ? D3D12_COLOR_WRITE_ENABLE_RED : 0) |
				(colorWriteMode.G == re::TextureTarget::TargetParams::ChannelWrite::Mode::Enabled ? D3D12_COLOR_WRITE_ENABLE_GREEN : 0) |
				(colorWriteMode.B == re::TextureTarget::TargetParams::ChannelWrite::Mode::Enabled ? D3D12_COLOR_WRITE_ENABLE_BLUE : 0) |
				(colorWriteMode.A == re::TextureTarget::TargetParams::ChannelWrite::Mode::Enabled ? D3D12_COLOR_WRITE_ENABLE_ALPHA : 0);

			blendDesc.RenderTarget[i] = rtBlendDesc;
		}

		return blendDesc;
	}

	
	constexpr D3D12_PRIMITIVE_TOPOLOGY_TYPE GetD3DTopologyType(re::PipelineState::TopologyType topologyType)
	{
		switch (topologyType)
		{
		case re::PipelineState::TopologyType::Point: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		case re::PipelineState::TopologyType::Line: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case re::PipelineState::TopologyType::Triangle: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		case re::PipelineState::TopologyType::Patch: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
		default:
			SEAssertF("Invalid topology type");
		}
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
	}
}


namespace dx12
{
	PipelineState::PipelineState()
		: m_pipelineState(nullptr)
	{
	}


	void PipelineState::Create(re::Shader const& shader, re::TextureTargetSet const& targetSet)
	{
		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

		// Generate the PSO:
		dx12::Shader::PlatformParams* shaderParams = shader.GetPlatformParams()->As<dx12::Shader::PlatformParams*>();

		re::PipelineState const& rePipelineState = shader.GetPipelineState();

		if (shaderParams->m_shaderBlobs[dx12::Shader::Vertex]) // Vertex shader is mandatory for graphics pipelines
		{
			// Build the vertex stream input layout:
			std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
			BuildInputLayout(shaderParams, inputLayout);

			// Build graphics pipeline description:
			GraphicsPipelineStateStream pipelineStateStream {};
			pipelineStateStream.rootSignature = shaderParams->m_rootSignature->GetD3DRootSignature();
			pipelineStateStream.inputLayout = { inputLayout.data(), static_cast<uint32_t>(inputLayout.size())};
			pipelineStateStream.primitiveTopologyType = GetD3DTopologyType(rePipelineState.GetTopologyType());
			pipelineStateStream.vShader = CD3DX12_SHADER_BYTECODE(shaderParams->m_shaderBlobs[Shader::Vertex].Get());

			if (shaderParams->m_shaderBlobs[dx12::Shader::Geometry])
			{
				pipelineStateStream.gShader = CD3DX12_SHADER_BYTECODE(shaderParams->m_shaderBlobs[Shader::Geometry].Get());
			}
			
			if (shaderParams->m_shaderBlobs[dx12::Shader::Pixel])
			{
				pipelineStateStream.pShader = CD3DX12_SHADER_BYTECODE(shaderParams->m_shaderBlobs[Shader::Pixel].Get());
			}

			SEAssert("TODO: Support this shader type", 
				!shaderParams->m_shaderBlobs[dx12::Shader::Hull] && 
				!shaderParams->m_shaderBlobs[dx12::Shader::Domain] &&
				!shaderParams->m_shaderBlobs[dx12::Shader::Mesh] &&
				!shaderParams->m_shaderBlobs[dx12::Shader::Amplification]);

			// Target formats:
			dx12::TextureTargetSet::PlatformParams* targetSetPlatParams =
				targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();
			if (targetSet.HasColorTarget())
			{
				pipelineStateStream.RTVFormats = TextureTargetSet::GetColorTargetFormats(targetSet);
			}
			if (targetSet.HasDepthTarget())
			{
				pipelineStateStream.DSVFormat =
					targetSet.GetDepthStencilTarget()->GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>()->m_format;
			}			

			// Rasterizer description:
			const D3D12_RASTERIZER_DESC rasterizerDesc = BuildRasterizerDesc(rePipelineState);
			pipelineStateStream.rasterizer = CD3DX12_RASTERIZER_DESC(rasterizerDesc);

			// Depth stencil description:
			const D3D12_DEPTH_STENCIL_DESC depthStencilDesc = 
				BuildDepthStencilDesc(targetSet.GetDepthStencilTarget(), rePipelineState);
			pipelineStateStream.depthStencil = CD3DX12_DEPTH_STENCIL_DESC(depthStencilDesc);

			// Blend description:
			const D3D12_BLEND_DESC blendDesc = BuildBlendDesc(targetSet, rePipelineState);
			pipelineStateStream.blend = CD3DX12_BLEND_DESC(blendDesc);

			const D3D12_PIPELINE_STATE_STREAM_DESC graphicsPipelineStateStreamDesc =
			{
				sizeof(GraphicsPipelineStateStream),
				&pipelineStateStream
			};

			// CreatePipelineState can create both graphics & compute pipelines from a D3D12_PIPELINE_STATE_STREAM_DESC
			HRESULT hr = device->CreatePipelineState(&graphicsPipelineStateStreamDesc, IID_PPV_ARGS(&m_pipelineState));
			CheckHResult(hr, "Failed to create graphics pipeline state");

			// Name our PSO:
			const std::wstring graphicsPipelineStateName = shader.GetWName() + L"_" + targetSet.GetWName() + L"_PSO";
			m_pipelineState->SetName(graphicsPipelineStateName.c_str());
		}
		else if (shaderParams->m_shaderBlobs[dx12::Shader::Compute])
		{
			// Build compute pipeline description:
			ComputePipelineStateStream computePipelineStateStream;
			computePipelineStateStream.rootSignature = shaderParams->m_rootSignature->GetD3DRootSignature();
			computePipelineStateStream.cShader = CD3DX12_SHADER_BYTECODE(shaderParams->m_shaderBlobs[Shader::Compute].Get());

			const D3D12_PIPELINE_STATE_STREAM_DESC computePipelineStateStreamDesc
			{
				sizeof(ComputePipelineStateStream),
				&computePipelineStateStream
			};

			// CreatePipelineState can create both graphics & compute pipelines from a D3D12_PIPELINE_STATE_STREAM_DESC
			HRESULT hr = device->CreatePipelineState(&computePipelineStateStreamDesc, IID_PPV_ARGS(&m_pipelineState));
			CheckHResult(hr, "Failed to create compute pipeline state");

			// Name our PSO:
			const std::wstring pipelineStateName = shader.GetWName() + L"_Compute_PSO";
			m_pipelineState->SetName(pipelineStateName.c_str());
		}
		else
		{
			SEAssertF("Shader doesn't have a supported combination of shader blobs. TODO: Support this");
		}
	}


	void PipelineState::Destroy()
	{
		m_pipelineState = nullptr;		
	}


	ID3D12PipelineState* PipelineState::GetD3DPipelineState() const
	{
		return m_pipelineState.Get();
	}
}