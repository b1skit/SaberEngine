// © 2022 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "EnumTypes_DX12.h"
#include "RasterState.h"
#include "RenderManager.h"
#include "PipelineState_DX12.h"
#include "RootSignature_DX12.h"
#include "Shader.h"
#include "Shader_DX12.h"
#include "SysInfo_DX12.h"
#include "Texture_DX12.h"
#include "TextureTarget.h"
#include "TextureTarget_DX12.h"

#include "Core/Assert.h"
#include "Core/Util/TextUtils.h"


using Microsoft::WRL::ComPtr;

namespace
{
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


	inline constexpr char const* VertexStreamTypeToSemanticName(re::VertexStream::Type streamType, uint8_t semanticIdx)
	{
		switch (streamType)
		{
		case re::VertexStream::Type::Position:
		{
			if (semanticIdx == 0)
			{
				return "SV_Position";
			}
			return "POSITION";
		}
		break;
		case re::VertexStream::Type::Normal: return "NORMAL";
		//case re::VertexStream::Type::Binormal: return "BINORMAL";
		case re::VertexStream::Type::Tangent: return "TANGENT";
		case re::VertexStream::Type::TexCoord: return "TEXCOORD";
		case re::VertexStream::Type::Color: return "COLOR";
		case re::VertexStream::Type::BlendIndices: return "BLENDINDICES";
		case re::VertexStream::Type::BlendWeight: return "BLENDWEIGHT";
		//case re::VertexStream::Type::PointSize: return "PSIZE";
		default: return "INVALID_RE_VERTEX_STREAM_TYPE";
		}
		SEStaticAssert(re::VertexStream::Type_Count == 8, "Number of vertex stream types changed. This must be updated");
	}


	std::vector<D3D12_INPUT_ELEMENT_DESC> BuildInputLayout(re::Shader const& shader)
	{
		// Get the vertex stream metadata, and the number of attributes it points to:
		uint8_t numVertexAttributes = 0;
		re::VertexStreamMap::StreamMetadata const* vertexStreamMetadata = 
			shader.GetVertexStreamMap()->GetStreamMetadata(numVertexAttributes);

		std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
		inputLayout.reserve(numVertexAttributes);

		for (uint8_t i = 0; i < numVertexAttributes; ++i)
		{
			re::VertexStreamMap::StreamMetadata const& entry = vertexStreamMetadata[i];

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


	D3D12_RASTERIZER_DESC BuildRasterizerDesc(re::RasterState const* rasterState)
	{
		D3D12_RASTERIZER_DESC rasterizerDesc{};

		// Polygon fill mode:
		switch (rasterState->GetFillMode())
		{
		case re::RasterState::FillMode::Wireframe: rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME; break;
		case re::RasterState::FillMode::Solid: rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; break;
		default:
			SEAssertF("Invalid fill mode");
			rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		}

		// Face culling mode:
		switch (rasterState->GetFaceCullingMode())
		{
		case re::RasterState::FaceCullingMode::Disabled: rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; break;
		case re::RasterState::FaceCullingMode::Front: rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT; break;
		case re::RasterState::FaceCullingMode::Back: rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK; break;			
		default:
			SEAssertF("Invalid cull mode");
			rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
		}

		// Winding order:
		switch (rasterState->GetWindingOrder())
		{
		case re::RasterState::WindingOrder::CCW: rasterizerDesc.FrontCounterClockwise = true; break;
		case re::RasterState::WindingOrder::CW: rasterizerDesc.FrontCounterClockwise = false; break;
		default:
			SEAssertF("Invalid winding order");
			rasterizerDesc.FrontCounterClockwise = true;
		}
		
		rasterizerDesc.DepthBias = rasterState->GetDepthBias();
		rasterizerDesc.DepthBiasClamp = rasterState->GetDepthBiasClamp();
		rasterizerDesc.SlopeScaledDepthBias = rasterState->GetSlopeScaledDepthBias();
		rasterizerDesc.DepthClipEnable = rasterState->GetDepthClipEnabled();
		rasterizerDesc.MultisampleEnable = rasterState->GetMultiSampleEnabled();
		rasterizerDesc.AntialiasedLineEnable = rasterState->GetAntiAliasedLineEnabled(); // Only applies if drawing lines with .MultisampleEnable = false
		rasterizerDesc.ForcedSampleCount = rasterState->GetForcedSampleCount();
		rasterizerDesc.ConservativeRaster = rasterState->GetConservativeRaster() ? 
			D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		return rasterizerDesc;
	}


	constexpr D3D12_DEPTH_WRITE_MASK DepthWriteMaskToD3DDepthWriteMask(re::RasterState::DepthWriteMask depthWriteMask)
	{
		switch (depthWriteMask)
		{
		case re::RasterState::DepthWriteMask::Zero: return D3D12_DEPTH_WRITE_MASK_ZERO;
		case re::RasterState::DepthWriteMask::All: return D3D12_DEPTH_WRITE_MASK_ALL;
		}
		return D3D12_DEPTH_WRITE_MASK_ALL; // This should never happen
	}


	constexpr D3D12_STENCIL_OP StencilOpToD3DStencilOp(re::RasterState::StencilOp stencilOp)
	{
		switch (stencilOp)
		{
		case re::RasterState::StencilOp::Keep: return D3D12_STENCIL_OP_KEEP;
		case re::RasterState::StencilOp::Zero: return D3D12_STENCIL_OP_ZERO;
		case re::RasterState::StencilOp::Replace: return D3D12_STENCIL_OP_REPLACE;
		case re::RasterState::StencilOp::IncrementSaturate: return D3D12_STENCIL_OP_INCR_SAT;
		case re::RasterState::StencilOp::DecrementSaturate: return D3D12_STENCIL_OP_DECR_SAT;
		case re::RasterState::StencilOp::Invert: return D3D12_STENCIL_OP_INVERT;
		case re::RasterState::StencilOp::Increment: return D3D12_STENCIL_OP_INCR;
		case re::RasterState::StencilOp::Decrement: return D3D12_STENCIL_OP_DECR;
		}
		return D3D12_STENCIL_OP_KEEP; // This should never happen
	}


	constexpr D3D12_COMPARISON_FUNC ComparisonFuncToD3DComparisonFunc(re::RasterState::ComparisonFunc comparison)
	{
		switch (comparison)
		{
		case re::RasterState::ComparisonFunc::Less: return D3D12_COMPARISON_FUNC_LESS;
		case re::RasterState::ComparisonFunc::Never: return D3D12_COMPARISON_FUNC_NEVER;
		case re::RasterState::ComparisonFunc::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
		case re::RasterState::ComparisonFunc::LEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case re::RasterState::ComparisonFunc::Greater: return D3D12_COMPARISON_FUNC_GREATER;
		case re::RasterState::ComparisonFunc::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
		case re::RasterState::ComparisonFunc::GEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		case re::RasterState::ComparisonFunc::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
		}
		return D3D12_COMPARISON_FUNC_NONE; // This should never happen
	}


	D3D12_DEPTH_STENCILOP_DESC StencilOpDescToD3DStencilOpDesc(re::RasterState::StencilOpDesc const& stencilOpDesc)
	{
		return D3D12_DEPTH_STENCILOP_DESC{
			.StencilFailOp = StencilOpToD3DStencilOp(stencilOpDesc.m_failOp),
			.StencilDepthFailOp = StencilOpToD3DStencilOp(stencilOpDesc.m_depthFailOp),
			.StencilPassOp = StencilOpToD3DStencilOp(stencilOpDesc.m_passOp),
			.StencilFunc = ComparisonFuncToD3DComparisonFunc(stencilOpDesc.m_comparison),
		};
	}


	D3D12_DEPTH_STENCIL_DESC BuildDepthStencilDesc(re::RasterState const* rasterState)
	{
		// We make assumptions when recording resource transitions on our command lists that depth targets will 
		// specifically have depth disabled (not just masked out) when the depth channel write mode is disabled
		SEAssert(rasterState->GetDepthTestEnabled() ||
			(!rasterState->GetDepthTestEnabled() &&
				(rasterState->GetDepthWriteMask() == re::RasterState::DepthWriteMask::Zero)),
			"Depth test state does not match the write mask state");

		return D3D12_DEPTH_STENCIL_DESC {
			.DepthEnable = rasterState->GetDepthTestEnabled(),
			.DepthWriteMask = DepthWriteMaskToD3DDepthWriteMask(rasterState->GetDepthWriteMask()),
			.DepthFunc = ComparisonFuncToD3DComparisonFunc(rasterState->GetDepthComparison()),
			.StencilEnable = rasterState->GetStencilEnabled(),
			.StencilReadMask = rasterState->GetStencilReadMask(),
			.StencilWriteMask = rasterState->GetStencilWriteMask(),
			.FrontFace = StencilOpDescToD3DStencilOpDesc(rasterState->GetFrontFaceStencilOpDesc()),
			.BackFace = StencilOpDescToD3DStencilOpDesc(rasterState->GetBackFaceStencilOpDesc()),
		};
	}


	constexpr D3D12_BLEND BlendModeToD3DBlendMode(re::RasterState::BlendMode blendMode)
	{
		switch (blendMode)
		{
			case re::RasterState::BlendMode::Zero: return D3D12_BLEND_ZERO;
			case re::RasterState::BlendMode::One: return D3D12_BLEND_ONE;
			case re::RasterState::BlendMode::SrcColor: return D3D12_BLEND_SRC_COLOR;
			case re::RasterState::BlendMode::InvSrcColor: return D3D12_BLEND_INV_SRC_COLOR;
			case re::RasterState::BlendMode::SrcAlpha: return D3D12_BLEND_SRC_ALPHA;
			case re::RasterState::BlendMode::InvSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
			case re::RasterState::BlendMode::DstAlpha: return D3D12_BLEND_DEST_ALPHA;
			case re::RasterState::BlendMode::InvDstAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
			case re::RasterState::BlendMode::DstColor: return D3D12_BLEND_DEST_COLOR;
			case re::RasterState::BlendMode::InvDstColor: return D3D12_BLEND_INV_DEST_COLOR;
			case re::RasterState::BlendMode::SrcAlphaSat: return D3D12_BLEND_SRC_ALPHA_SAT;
			case re::RasterState::BlendMode::BlendFactor: return D3D12_BLEND_BLEND_FACTOR;
			case re::RasterState::BlendMode::InvBlendFactor: return D3D12_BLEND_INV_BLEND_FACTOR;
			case re::RasterState::BlendMode::SrcOneColor: return D3D12_BLEND_SRC1_COLOR;
			case re::RasterState::BlendMode::InvSrcOneColor: return D3D12_BLEND_INV_SRC1_COLOR;
			case re::RasterState::BlendMode::SrcOneAlpha: return D3D12_BLEND_SRC1_ALPHA;
			case re::RasterState::BlendMode::InvSrcOneAlpha: return D3D12_BLEND_INV_SRC1_ALPHA;
			case re::RasterState::BlendMode::AlphaFactor: return D3D12_BLEND_ALPHA_FACTOR;
			case re::RasterState::BlendMode::InvAlphaFactor: return D3D12_BLEND_INV_ALPHA_FACTOR;
		}
		return D3D12_BLEND_ONE; // This should never happen
	}


	constexpr D3D12_BLEND_OP BlendOpToD3DBlendOp(re::RasterState::BlendOp blendOp)
	{
		switch (blendOp)
		{
			case re::RasterState::BlendOp::Add: return D3D12_BLEND_OP_ADD;
			case re::RasterState::BlendOp::Subtract: return D3D12_BLEND_OP_SUBTRACT;
			case re::RasterState::BlendOp::RevSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
			case re::RasterState::BlendOp::Min: return D3D12_BLEND_OP_MIN;
			case re::RasterState::BlendOp::Max: return D3D12_BLEND_OP_MAX;
		}
		return D3D12_BLEND_OP_ADD; // This should never happen
	}


	constexpr D3D12_LOGIC_OP LogicOpToD3DLogicOp(re::RasterState::LogicOp logicOp)
	{
		switch (logicOp)
		{
			case re::RasterState::LogicOp::Clear: return D3D12_LOGIC_OP_CLEAR;
			case re::RasterState::LogicOp::Set: return D3D12_LOGIC_OP_SET;
			case re::RasterState::LogicOp::Copy: return D3D12_LOGIC_OP_COPY;
			case re::RasterState::LogicOp::CopyInverted: return D3D12_LOGIC_OP_COPY_INVERTED;
			case re::RasterState::LogicOp::NoOp: return D3D12_LOGIC_OP_NOOP;
			case re::RasterState::LogicOp::Invert: return D3D12_LOGIC_OP_INVERT;
			case re::RasterState::LogicOp::AND: return D3D12_LOGIC_OP_AND;
			case re::RasterState::LogicOp::NAND: return D3D12_LOGIC_OP_NAND;
			case re::RasterState::LogicOp::OR: return D3D12_LOGIC_OP_OR;
			case re::RasterState::LogicOp::NOR: return D3D12_LOGIC_OP_NOR;
			case re::RasterState::LogicOp::XOR: return D3D12_LOGIC_OP_XOR;
			case re::RasterState::LogicOp::EQUIV: return D3D12_LOGIC_OP_EQUIV;
			case re::RasterState::LogicOp::ANDReverse: return D3D12_LOGIC_OP_AND_REVERSE;
			case re::RasterState::LogicOp::AndInverted: return D3D12_LOGIC_OP_AND_INVERTED;
			case re::RasterState::LogicOp::ORReverse: return D3D12_LOGIC_OP_OR_REVERSE;
			case re::RasterState::LogicOp::ORInverted: return D3D12_LOGIC_OP_OR_INVERTED;
		}
		return D3D12_LOGIC_OP_NOOP; // This should never happen
	}


	D3D12_BLEND_DESC BuildBlendDesc(re::RasterState const& rasterState)
	{
		D3D12_BLEND_DESC blendDesc{};

		blendDesc.AlphaToCoverageEnable = rasterState.GetAlphaToCoverageEnabled();
		blendDesc.IndependentBlendEnable = rasterState.GetIndependentBlendEnabled();

		// Configure the blend mode for each target:
		for (uint32_t i = 0; i < dx12::SysInfo::GetMaxRenderTargets(); i++)
		{
			re::RasterState::RenderTargetBlendDesc const& reBlendDesc = rasterState.GetRenderTargetBlendDescs()[i];

			D3D12_RENDER_TARGET_BLEND_DESC& rtBlendDesc = blendDesc.RenderTarget[i];

			rtBlendDesc.BlendEnable = reBlendDesc.m_blendEnable;
			rtBlendDesc.SrcBlend = BlendModeToD3DBlendMode(reBlendDesc.m_srcBlend);
			rtBlendDesc.DestBlend = BlendModeToD3DBlendMode(reBlendDesc.m_dstBlend);
			rtBlendDesc.LogicOpEnable = reBlendDesc.m_logicOpEnable;
			rtBlendDesc.BlendOp = BlendOpToD3DBlendOp(reBlendDesc.m_blendOp);
			rtBlendDesc.SrcBlendAlpha = BlendModeToD3DBlendMode(reBlendDesc.m_srcBlendAlpha);
			rtBlendDesc.DestBlendAlpha = BlendModeToD3DBlendMode(reBlendDesc.m_dstBlendAlpha);
			rtBlendDesc.BlendOpAlpha = BlendOpToD3DBlendOp(reBlendDesc.m_blendOpAlpha);
			rtBlendDesc.LogicOp = LogicOpToD3DLogicOp(reBlendDesc.m_logicOp);
			rtBlendDesc.RenderTargetWriteMask = reBlendDesc.m_renderTargetWriteMask;
		}

		return blendDesc;
	}

	
	constexpr D3D12_PRIMITIVE_TOPOLOGY_TYPE GetD3DTopologyType(re::RasterState::PrimitiveTopologyType topologyType)
	{
		switch (topologyType)
		{
		case re::RasterState::PrimitiveTopologyType::Point: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		case re::RasterState::PrimitiveTopologyType::Line: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case re::RasterState::PrimitiveTopologyType::Triangle: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		case re::RasterState::PrimitiveTopologyType::Patch: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
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
		// Generate the PSO:
		dx12::Shader::PlatObj* shaderPlatObj = shader.GetPlatformObject()->As<dx12::Shader::PlatObj*>();
		
		SEAssert(!shaderPlatObj->m_shaderBlobs[re::Shader::Hull] &&
			!shaderPlatObj->m_shaderBlobs[re::Shader::Domain] &&
			!shaderPlatObj->m_shaderBlobs[re::Shader::Mesh] &&
			!shaderPlatObj->m_shaderBlobs[re::Shader::Amplification],
			"TODO: Support this shader type");

		Microsoft::WRL::ComPtr<ID3D12Device> device =
			shaderPlatObj->GetContext()->As<dx12::Context*>()->GetDevice().GetD3DDevice();

		Microsoft::WRL::ComPtr<ID3D12Device2> device2;
		HRESULT hr = device.As(&device2);
		SEAssert(SUCCEEDED(hr), "Failed to get ID3D12Device2 from ID3D12Device");

		if (shaderPlatObj->m_shaderBlobs[re::Shader::Vertex]) // Vertex shader is mandatory for graphics pipelines
		{
			SEAssert(targetSet, "Raster pipelines require a valid target set");

			re::RasterState const* rasterState = shader.GetRasterizationState();

			// Get the shader reflection:
			ComPtr<IDxcUtils> dxcUtils;
			hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
			dx12::CheckHResult(hr, "Failed to create IDxcUtils instance");

			const DxcBuffer reflectionBuffer
			{
				.Ptr = shaderPlatObj->m_shaderBlobs[re::Shader::Vertex]->GetBufferPointer(),
				.Size = shaderPlatObj->m_shaderBlobs[re::Shader::Vertex]->GetBufferSize(),
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
			GraphicsPipelineStateStream graphicsStateStream {};
			graphicsStateStream.rootSignature = shaderPlatObj->m_rootSignature->GetD3DRootSignature();
			graphicsStateStream.inputLayout = { inputLayout.data(), static_cast<uint32_t>(inputLayout.size())};
			graphicsStateStream.primitiveTopologyType = GetD3DTopologyType(rasterState->GetPrimitiveTopologyType());
			graphicsStateStream.vShader = CD3DX12_SHADER_BYTECODE(shaderPlatObj->m_shaderBlobs[re::Shader::Vertex].Get());

			if (shaderPlatObj->m_shaderBlobs[re::Shader::Geometry])
			{
				graphicsStateStream.gShader = CD3DX12_SHADER_BYTECODE(shaderPlatObj->m_shaderBlobs[re::Shader::Geometry].Get());
			}
			
			if (shaderPlatObj->m_shaderBlobs[re::Shader::Pixel])
			{
				graphicsStateStream.pShader = CD3DX12_SHADER_BYTECODE(shaderPlatObj->m_shaderBlobs[re::Shader::Pixel].Get());
			}

			// Target formats:
			dx12::TextureTargetSet::PlatObj* targetSetPlatObj =
				targetSet->GetPlatformObject()->As<dx12::TextureTargetSet::PlatObj*>();
			if (targetSet->HasColorTarget())
			{
				graphicsStateStream.RTVFormats = TextureTargetSet::GetColorTargetFormats(*targetSet);
			}
			if (targetSet->HasDepthTarget())
			{
				graphicsStateStream.DSVFormat =
					targetSet->GetDepthStencilTarget().GetTexture()->GetPlatformObject()->As<dx12::Texture::PlatObj*>()->m_format;
			}			

			// Rasterizer description:
			D3D12_RASTERIZER_DESC const& rasterizerDesc = BuildRasterizerDesc(rasterState);
			graphicsStateStream.rasterizer = CD3DX12_RASTERIZER_DESC(rasterizerDesc);

			// Depth stencil description:
			graphicsStateStream.depthStencil = CD3DX12_DEPTH_STENCIL_DESC(BuildDepthStencilDesc(rasterState));

			// Blend description:
			D3D12_BLEND_DESC const& blendDesc = BuildBlendDesc(*rasterState);
			graphicsStateStream.blend = CD3DX12_BLEND_DESC(blendDesc);

			const D3D12_PIPELINE_STATE_STREAM_DESC graphicsPipelineStateStreamDesc =
			{
				sizeof(GraphicsPipelineStateStream),
				&graphicsStateStream
			};

			// CreatePipelineState can create both graphics & compute pipelines from a D3D12_PIPELINE_STATE_STREAM_DESC
			hr = device2->CreatePipelineState(&graphicsPipelineStateStreamDesc, IID_PPV_ARGS(&m_pipelineState));
			CheckHResult(hr, "Failed to create graphics pipeline state");

			// Name our PSO:
			m_pipelineState->SetName(util::ToWideString(
				std::format("{}_{}_GraphicsPSO",
					shader.GetName(),
					targetSet ? targetSet->GetName().c_str() : "<no targets>")).c_str());
		}
		else if (shaderPlatObj->m_shaderBlobs[re::Shader::Compute])
		{
			// Build compute pipeline description:
			ComputePipelineStateStream computePipelineStateStream;
			computePipelineStateStream.rootSignature = shaderPlatObj->m_rootSignature->GetD3DRootSignature();
			computePipelineStateStream.cShader = CD3DX12_SHADER_BYTECODE(shaderPlatObj->m_shaderBlobs[re::Shader::Compute].Get());

			const D3D12_PIPELINE_STATE_STREAM_DESC computePipelineStateStreamDesc
			{
				sizeof(ComputePipelineStateStream),
				&computePipelineStateStream
			};

			// CreatePipelineState can create both graphics & compute pipelines from a D3D12_PIPELINE_STATE_STREAM_DESC
			HRESULT hr = device2->CreatePipelineState(&computePipelineStateStreamDesc, IID_PPV_ARGS(&m_pipelineState));
			CheckHResult(hr, "Failed to create compute pipeline state");

			m_pipelineState->SetName(util::ToWideString(std::format("{}_ComputePSO", shader.GetName())).c_str());
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