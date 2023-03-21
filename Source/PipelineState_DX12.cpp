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


	D3D12_RASTERIZER_DESC BuildRasterizerDesc()
	{
		D3D12_RASTERIZER_DESC rasterizerDesc{};
		rasterizerDesc.FillMode = D3D12_FILL_MODE::D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = D3D12_CULL_MODE::D3D12_CULL_MODE_BACK;
		rasterizerDesc.FrontCounterClockwise = true;
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
}


namespace dx12
{
	PipelineState::PipelineState(
		gr::PipelineState const& grPipelineState, 
		re::Shader const* shader, 
		D3D12_RT_FORMAT_ARRAY const& rtvFormats,
		const DXGI_FORMAT dsvFormat)
		: m_pipelineState(nullptr)
	{
		SEAssert("Arguments cannot be null", shader);

		dx12::Context::PlatformParams* ctxPlatParams = 
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();
		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();


		// Generate the PSO:
		dx12::Shader::PlatformParams* shaderParams = shader->GetPlatformParams()->As<dx12::Shader::PlatformParams*>();

		SEAssert("Shader doesn't have a pixel and vertex shader blob. TODO: Support this", 
			shaderParams->m_shaderBlobs[dx12::Shader::Vertex] && shaderParams->m_shaderBlobs[dx12::Shader::Pixel]);

		if (shaderParams->m_shaderBlobs[dx12::Shader::Vertex] != nullptr)
		{
			// Build the input layout:
			std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
			BuildInputLayout(shaderParams, inputLayout);

			// Build the rasterizer description:
			D3D12_RASTERIZER_DESC rasterizerDesc = BuildRasterizerDesc();

			// Populate the pipeline state stream helper:
			PipelineStateStream pipelineStateStream;

			pipelineStateStream.rootSignature = m_rootSignature.GetD3DRootSignature();
			pipelineStateStream.inputLayout = { &inputLayout[0], static_cast<uint32_t>(inputLayout.size()) };
			pipelineStateStream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateStream.vShader = CD3DX12_SHADER_BYTECODE(shaderParams->m_shaderBlobs[dx12::Shader::Vertex].Get());
			pipelineStateStream.pShader = CD3DX12_SHADER_BYTECODE(shaderParams->m_shaderBlobs[dx12::Shader::Pixel].Get());

			pipelineStateStream.RTVFormats = rtvFormats;
			pipelineStateStream.DSVFormat = dsvFormat;
			
			pipelineStateStream.rasterizer = CD3DX12_RASTERIZER_DESC(rasterizerDesc);

			D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc =
			{
				sizeof(PipelineStateStream),
				&pipelineStateStream
			};
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