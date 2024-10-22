// © 2022 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "EnumTypes_DX12.h"
#include "MeshPrimitive.h"
#include "PipelineState.h"
#include "PipelineState_DX12.h"
#include "Shader.h"
#include "Shader_DX12.h"
#include "SysInfo_DX12.h"
#include "Texture_DX12.h"
#include "TextureTarget.h"
#include "TextureTarget_DX12.h"

#include "Core/Assert.h"
#include "Core/Util/TextUtils.h"

#include <d3dx12.h>
#include <dxcapi.h>
#include <d3d12shader.h>

using Microsoft::WRL::ComPtr;


namespace
{
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


	inline constexpr char const* VertexStreamTypeToSemanticName(gr::VertexStream::Type streamType, uint8_t semanticIdx)
	{
		switch (streamType)
		{
		case gr::VertexStream::Type::Position:
		{
			if (semanticIdx == 0)
			{
				return "SV_Position";
			}
			return "POSITION";
		}
		break;
		case gr::VertexStream::Type::Normal: return "NORMAL";
		case gr::VertexStream::Type::Binormal: return "BINORMAL";
		case gr::VertexStream::Type::Tangent: return "TANGENT";
		case gr::VertexStream::Type::TexCoord: return "TEXCOORD";
		case gr::VertexStream::Type::Color: return "COLOR";
		case gr::VertexStream::Type::BlendIndices: return "BLENDINDICES";
		case gr::VertexStream::Type::BlendWeight: return "BLENDWEIGHT";
		//case gr::VertexStream::Type::PointSize: return "PSIZE";
		default: return "INVALID_RE_VERTEX_STREAM_TYPE";
		}
	}


	std::vector<D3D12_INPUT_ELEMENT_DESC> BuildInputLayout(re::Shader const& shader)
	{
		// Get the vertex stream metadata, and the number of attributes it points to:
		uint8_t numVertexAttributes = 0;
		re::VertexStreamMap::VertexStreamMetadata const* vertexStreamMetadata = 
			shader.GetVertexStreamMap()->GetStreamMetadata(numVertexAttributes);

		std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
		inputLayout.reserve(numVertexAttributes);

		for (uint8_t i = 0; i < numVertexAttributes; ++i)
		{
			re::VertexStreamMap::VertexStreamMetadata const& entry = vertexStreamMetadata[i];

			inputLayout.emplace_back(D3D12_INPUT_ELEMENT_DESC(
				VertexStreamTypeToSemanticName(
					entry.m_streamKey.m_streamType, entry.m_streamKey.m_semanticIdx),	// Semantic name
				entry.m_streamKey.m_semanticIdx,										// Semantic idx
				dx12::DataTypeToDXGI_FORMAT(entry.m_streamDataType, false),				// Format
				entry.m_shaderSlotIdx,													// Input slot [0, 15]
				D3D12_APPEND_ALIGNED_ELEMENT,											// Aligned byte offset
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,								// Input slot class
				0));																	// Input data step rate
		}

		return inputLayout;
	}


	D3D12_RASTERIZER_DESC BuildRasterizerDesc(re::PipelineState const* rePipelineState)
	{
		D3D12_RASTERIZER_DESC rasterizerDesc{};

		// Polygon fill mode:
		switch (rePipelineState->GetFillMode())
		{
		case re::PipelineState::FillMode::Wireframe: rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME; break;
		case re::PipelineState::FillMode::Solid: rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; break;
		default:
			SEAssertF("Invalid fill mode");
			rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		}

		// Face culling mode:
		switch (rePipelineState->GetFaceCullingMode())
		{
		case re::PipelineState::FaceCullingMode::Disabled: rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; break;
		case re::PipelineState::FaceCullingMode::Front: rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT; break;
		case re::PipelineState::FaceCullingMode::Back: rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK; break;			
		default:
			SEAssertF("Invalid cull mode");
			rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
		}

		// Winding order:
		switch (rePipelineState->GetWindingOrder())
		{
		case re::PipelineState::WindingOrder::CCW: rasterizerDesc.FrontCounterClockwise = true; break;
		case re::PipelineState::WindingOrder::CW: rasterizerDesc.FrontCounterClockwise = false; break;
		default:
			SEAssertF("Invalid winding order");
			rasterizerDesc.FrontCounterClockwise = true;
		}
		
		rasterizerDesc.DepthBias = rePipelineState->GetDepthBias();
		rasterizerDesc.DepthBiasClamp = rePipelineState->GetDepthBiasClamp();
		rasterizerDesc.SlopeScaledDepthBias = rePipelineState->GetSlopeScaledDepthBias();
		rasterizerDesc.DepthClipEnable = rePipelineState->GetDepthClipEnabled();
		rasterizerDesc.MultisampleEnable = rePipelineState->GetMultiSampleEnabled();
		rasterizerDesc.AntialiasedLineEnable = rePipelineState->GetAntiAliasedLineEnabled(); // Only applies if drawing lines with .MultisampleEnable = false
		rasterizerDesc.ForcedSampleCount = rePipelineState->GetForcedSampleCount();
		rasterizerDesc.ConservativeRaster = rePipelineState->GetConservativeRaster() ? 
			D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		return rasterizerDesc;
	}


	constexpr D3D12_DEPTH_WRITE_MASK DepthWriteMaskToD3DDepthWriteMask(re::PipelineState::DepthWriteMask depthWriteMask)
	{
		switch (depthWriteMask)
		{
		case re::PipelineState::DepthWriteMask::Zero: return D3D12_DEPTH_WRITE_MASK_ZERO;
		case re::PipelineState::DepthWriteMask::All: return D3D12_DEPTH_WRITE_MASK_ALL;
		}
		return D3D12_DEPTH_WRITE_MASK_ALL; // This should never happen
	}


	constexpr D3D12_STENCIL_OP StencilOpToD3DStencilOp(re::PipelineState::StencilOp stencilOp)
	{
		switch (stencilOp)
		{
		case re::PipelineState::StencilOp::Keep: return D3D12_STENCIL_OP_KEEP;
		case re::PipelineState::StencilOp::Zero: return D3D12_STENCIL_OP_ZERO;
		case re::PipelineState::StencilOp::Replace: return D3D12_STENCIL_OP_REPLACE;
		case re::PipelineState::StencilOp::IncrementSaturate: return D3D12_STENCIL_OP_INCR_SAT;
		case re::PipelineState::StencilOp::DecrementSaturate: return D3D12_STENCIL_OP_DECR_SAT;
		case re::PipelineState::StencilOp::Invert: return D3D12_STENCIL_OP_INVERT;
		case re::PipelineState::StencilOp::Increment: return D3D12_STENCIL_OP_INCR;
		case re::PipelineState::StencilOp::Decrement: return D3D12_STENCIL_OP_DECR;
		}
		return D3D12_STENCIL_OP_KEEP; // This should never happen
	}


	constexpr D3D12_COMPARISON_FUNC ComparisonFuncToD3DComparisonFunc(re::PipelineState::ComparisonFunc comparison)
	{
		switch (comparison)
		{
		case re::PipelineState::ComparisonFunc::Less: return D3D12_COMPARISON_FUNC_LESS;
		case re::PipelineState::ComparisonFunc::Never: return D3D12_COMPARISON_FUNC_NEVER;
		case re::PipelineState::ComparisonFunc::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
		case re::PipelineState::ComparisonFunc::LEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case re::PipelineState::ComparisonFunc::Greater: return D3D12_COMPARISON_FUNC_GREATER;
		case re::PipelineState::ComparisonFunc::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
		case re::PipelineState::ComparisonFunc::GEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		case re::PipelineState::ComparisonFunc::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
		}
		return D3D12_COMPARISON_FUNC_NONE; // This should never happen
	}


	D3D12_DEPTH_STENCILOP_DESC StencilOpDescToD3DStencilOpDesc(re::PipelineState::StencilOpDesc const& stencilOpDesc)
	{
		return D3D12_DEPTH_STENCILOP_DESC{
			.StencilFailOp = StencilOpToD3DStencilOp(stencilOpDesc.m_failOp),
			.StencilDepthFailOp = StencilOpToD3DStencilOp(stencilOpDesc.m_depthFailOp),
			.StencilPassOp = StencilOpToD3DStencilOp(stencilOpDesc.m_passOp),
			.StencilFunc = ComparisonFuncToD3DComparisonFunc(stencilOpDesc.m_comparison),
		};
	}


	D3D12_DEPTH_STENCIL_DESC BuildDepthStencilDesc(re::PipelineState const* rePipelineState)
	{
		// We make assumptions when recording resource transitions on our command lists that depth targets will 
		// specifically have depth disabled (not just masked out) when the depth channel write mode is disabled
		SEAssert(rePipelineState->GetDepthTestEnabled() ||
			(!rePipelineState->GetDepthTestEnabled() &&
				(rePipelineState->GetDepthWriteMask() == re::PipelineState::DepthWriteMask::Zero)),
			"Depth test state does not match the write mask state");

		return D3D12_DEPTH_STENCIL_DESC {
			.DepthEnable = rePipelineState->GetDepthTestEnabled(),
			.DepthWriteMask = DepthWriteMaskToD3DDepthWriteMask(rePipelineState->GetDepthWriteMask()),
			.DepthFunc = ComparisonFuncToD3DComparisonFunc(rePipelineState->GetDepthComparison()),
			.StencilEnable = rePipelineState->GetStencilEnabled(),
			.StencilReadMask = rePipelineState->GetStencilReadMask(),
			.StencilWriteMask = rePipelineState->GetStencilWriteMask(),
			.FrontFace = StencilOpDescToD3DStencilOpDesc(rePipelineState->GetFrontFaceStencilOpDesc()),
			.BackFace = StencilOpDescToD3DStencilOpDesc(rePipelineState->GetBackFaceStencilOpDesc()),
		};
	}


	D3D12_BLEND_DESC BuildBlendDesc(re::TextureTargetSet const& targetSet)
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

			re::TextureTarget const& colorTarget = targetSet.GetColorTarget(i);

			re::TextureTarget::TargetParams::BlendModes const& blendModes = colorTarget.GetBlendMode();

			// Source blending:
			switch (blendModes.m_srcBlendMode)
			{
			case re::TextureTarget::BlendMode::Disabled:
			{
				SEAssert(blendModes.m_srcBlendMode == blendModes.m_dstBlendMode,
					"Must disable blending for both source and destination");

				rtBlendDesc.BlendEnable = false;
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_ZERO;
			}
			break;
			case re::TextureTarget::BlendMode::Zero:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_ZERO; break;
			case re::TextureTarget::BlendMode::One:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_ONE; break;
			case re::TextureTarget::BlendMode::SrcColor:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_SRC_COLOR; break;
			case re::TextureTarget::BlendMode::OneMinusSrcColor:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_INV_SRC_COLOR; break;
			case re::TextureTarget::BlendMode::DstColor:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_DEST_COLOR; break;
			case re::TextureTarget::BlendMode::OneMinusDstColor:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_INV_DEST_COLOR; break;
			case re::TextureTarget::BlendMode::SrcAlpha:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_SRC_ALPHA; break;
			case re::TextureTarget::BlendMode::OneMinusSrcAlpha:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_INV_SRC_ALPHA; break;
			case re::TextureTarget::BlendMode::DstAlpha:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_DEST_ALPHA; break;
			case re::TextureTarget::BlendMode::OneMinusDstAlpha:
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_INV_DEST_ALPHA; break;
			default:
				SEAssertF("Invalid source blend mode");
				rtBlendDesc.SrcBlend = D3D12_BLEND::D3D12_BLEND_ONE;
			}

			// Destination blending:
			switch (blendModes.m_dstBlendMode)
			{
			case re::TextureTarget::BlendMode::Disabled:
			{
				SEAssert(blendModes.m_srcBlendMode == blendModes.m_dstBlendMode,
					"Must disable blending for both source and destination");

				rtBlendDesc.BlendEnable = false;
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_ZERO;
			}
			break;
			case re::TextureTarget::BlendMode::Zero:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_ZERO; break;
			case re::TextureTarget::BlendMode::One:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_ONE; break;
			case re::TextureTarget::BlendMode::SrcColor:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_SRC_COLOR; break;
			case re::TextureTarget::BlendMode::OneMinusSrcColor:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_INV_SRC_COLOR; break;
			case re::TextureTarget::BlendMode::DstColor:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_DEST_COLOR; break;
			case re::TextureTarget::BlendMode::OneMinusDstColor:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_INV_DEST_COLOR; break;
			case re::TextureTarget::BlendMode::SrcAlpha:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_SRC_ALPHA; break;
			case re::TextureTarget::BlendMode::OneMinusSrcAlpha:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_INV_SRC_ALPHA; break;
			case re::TextureTarget::BlendMode::DstAlpha:
				rtBlendDesc.DestBlend = D3D12_BLEND::D3D12_BLEND_DEST_ALPHA; break;
			case re::TextureTarget::BlendMode::OneMinusDstAlpha:
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
			rtBlendDesc.RenderTargetWriteMask = static_cast<uint8_t>(colorTarget.GetColorWriteMask());

			blendDesc.RenderTarget[i] = rtBlendDesc;
		}

		return blendDesc;
	}

	
	constexpr D3D12_PRIMITIVE_TOPOLOGY_TYPE GetD3DTopologyType(re::PipelineState::PrimitiveTopologyType topologyType)
	{
		switch (topologyType)
		{
		case re::PipelineState::PrimitiveTopologyType::Point: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		case re::PipelineState::PrimitiveTopologyType::Line: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case re::PipelineState::PrimitiveTopologyType::Triangle: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		case re::PipelineState::PrimitiveTopologyType::Patch: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
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


	void PipelineState::Create(re::Shader const& shader, re::TextureTargetSet const* targetSet)
	{
		ID3D12Device2* device = re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDisplayDevice();

		// Generate the PSO:
		dx12::Shader::PlatformParams* shaderParams = shader.GetPlatformParams()->As<dx12::Shader::PlatformParams*>();
		
		SEAssert(!shaderParams->m_shaderBlobs[re::Shader::Hull] &&
			!shaderParams->m_shaderBlobs[re::Shader::Domain] &&
			!shaderParams->m_shaderBlobs[re::Shader::Mesh] &&
			!shaderParams->m_shaderBlobs[re::Shader::Amplification],
			"TODO: Support this shader type");

		if (shaderParams->m_shaderBlobs[re::Shader::Vertex]) // Vertex shader is mandatory for graphics pipelines
		{
			SEAssert(targetSet, "Graphics pipelines require a valid target set");

			re::PipelineState const* rePipelineState = shader.GetPipelineState();

			// Get the shader reflection:
			ComPtr<IDxcUtils> dxcUtils;
			HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
			dx12::CheckHResult(hr, "Failed to create IDxcUtils instance");

			const DxcBuffer reflectionBuffer
			{
				.Ptr = shaderParams->m_shaderBlobs[re::Shader::Vertex]->GetBufferPointer(),
				.Size = shaderParams->m_shaderBlobs[re::Shader::Vertex]->GetBufferSize(),
				.Encoding = 0,
			};

			ComPtr<ID3D12ShaderReflection> shaderReflection;
			hr = dxcUtils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&shaderReflection));
			dx12::CheckHResult(hr, "Failed to reflect shader");

			D3D12_SHADER_DESC shaderDesc{};
			hr = shaderReflection->GetDesc(&shaderDesc);
			dx12::CheckHResult(hr, "Failed to get shader description");


			// Build the vertex stream input layout using the shader reflection:
			std::vector<D3D12_INPUT_ELEMENT_DESC> const& inputLayout = BuildInputLayout(shader);

			// Build graphics pipeline description:
			GraphicsPipelineStateStream pipelineStateStream {};
			pipelineStateStream.rootSignature = shaderParams->m_rootSignature->GetD3DRootSignature();
			pipelineStateStream.inputLayout = { inputLayout.data(), static_cast<uint32_t>(inputLayout.size())};
			pipelineStateStream.primitiveTopologyType = GetD3DTopologyType(rePipelineState->GetPrimitiveTopologyType());
			pipelineStateStream.vShader = CD3DX12_SHADER_BYTECODE(shaderParams->m_shaderBlobs[re::Shader::Vertex].Get());

			if (shaderParams->m_shaderBlobs[re::Shader::Geometry])
			{
				pipelineStateStream.gShader = CD3DX12_SHADER_BYTECODE(shaderParams->m_shaderBlobs[re::Shader::Geometry].Get());
			}
			
			if (shaderParams->m_shaderBlobs[re::Shader::Pixel])
			{
				pipelineStateStream.pShader = CD3DX12_SHADER_BYTECODE(shaderParams->m_shaderBlobs[re::Shader::Pixel].Get());
			}

			// Target formats:
			dx12::TextureTargetSet::PlatformParams* targetSetPlatParams =
				targetSet->GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();
			if (targetSet->HasColorTarget())
			{
				pipelineStateStream.RTVFormats = TextureTargetSet::GetColorTargetFormats(*targetSet);
			}
			if (targetSet->HasDepthTarget())
			{
				pipelineStateStream.DSVFormat =
					targetSet->GetDepthStencilTarget().GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>()->m_format;
			}			

			// Rasterizer description:
			const D3D12_RASTERIZER_DESC rasterizerDesc = BuildRasterizerDesc(rePipelineState);
			pipelineStateStream.rasterizer = CD3DX12_RASTERIZER_DESC(rasterizerDesc);

			// Depth stencil description:
			pipelineStateStream.depthStencil = CD3DX12_DEPTH_STENCIL_DESC(BuildDepthStencilDesc(rePipelineState));

			// Blend description:
			const D3D12_BLEND_DESC blendDesc = BuildBlendDesc(*targetSet);
			pipelineStateStream.blend = CD3DX12_BLEND_DESC(blendDesc);

			const D3D12_PIPELINE_STATE_STREAM_DESC graphicsPipelineStateStreamDesc =
			{
				sizeof(GraphicsPipelineStateStream),
				&pipelineStateStream
			};

			// CreatePipelineState can create both graphics & compute pipelines from a D3D12_PIPELINE_STATE_STREAM_DESC
			hr = device->CreatePipelineState(&graphicsPipelineStateStreamDesc, IID_PPV_ARGS(&m_pipelineState));
			CheckHResult(hr, "Failed to create graphics pipeline state");
		}
		else if (shaderParams->m_shaderBlobs[re::Shader::Compute])
		{
			// Build compute pipeline description:
			ComputePipelineStateStream computePipelineStateStream;
			computePipelineStateStream.rootSignature = shaderParams->m_rootSignature->GetD3DRootSignature();
			computePipelineStateStream.cShader = CD3DX12_SHADER_BYTECODE(shaderParams->m_shaderBlobs[re::Shader::Compute].Get());

			const D3D12_PIPELINE_STATE_STREAM_DESC computePipelineStateStreamDesc
			{
				sizeof(ComputePipelineStateStream),
				&computePipelineStateStream
			};

			// CreatePipelineState can create both graphics & compute pipelines from a D3D12_PIPELINE_STATE_STREAM_DESC
			HRESULT hr = device->CreatePipelineState(&computePipelineStateStreamDesc, IID_PPV_ARGS(&m_pipelineState));
			CheckHResult(hr, "Failed to create compute pipeline state");
		}
		else
		{
			SEAssertF("Shader doesn't have a supported combination of shader blobs. TODO: Support this");
		}

		// Name our PSO:
		std::wstring const& psoDebugName = util::ToWideString(
			std::format("{}_{}_PSO",
				shader.GetName(), 
				targetSet ? targetSet->GetName() : "<no targets>"));
		m_pipelineState->SetName(psoDebugName.c_str());
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