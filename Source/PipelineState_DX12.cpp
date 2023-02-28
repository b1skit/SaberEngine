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

		const std::string semanticAndIndex = semantic + std::to_string(semanticIndex);

		auto const& result = k_sematicToSlot.find(semanticAndIndex);
		SEAssert("Invalid semantic and/or index", result != k_sematicToSlot.end());

		return result->second;
	}


	void BuildInputLayout(
		dx12::Shader::PlatformParams* const shaderParams, std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout)
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


	void GenerateRootSignature(ID3D12Device2* device, Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature)
	{
		// Create a root signature
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		// Allow input layout and deny unnecessary access to certain pipeline stages
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;


		// A single 32-bit constant root parameter that is used in our current HelloTriangle vertex shader
		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
		rootParameters[0].InitAsConstants(
			sizeof(glm::mat4) / 4,
			0,
			0,
			D3D12_SHADER_VISIBILITY_VERTEX);
		// TODO: ^^^ Handle populating root params (just use them for now)!!!!!!!!!!!!!!!!!!!!!!!!!!


		// Create the root signature description:
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

		// Serialize the root signature.
		ComPtr<ID3DBlob> rootSignatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3DX12SerializeVersionedRootSignature(
			&rootSignatureDescription,
			featureData.HighestVersion,
			&rootSignatureBlob,
			&errorBlob);
		CheckHResult(hr, "Failed to serialize versioned root signature");

		// Create the root signature.
		hr = device->CreateRootSignature(
			0,
			rootSignatureBlob->GetBufferPointer(),
			rootSignatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&rootSignature));
		CheckHResult(hr, "Failed to create root signature");
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

		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(re::RenderManager::Get()->GetContext().GetPlatformParams());
		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		// Generate the root signature:
		GenerateRootSignature(device, m_rootSignature);

		// Generate the PSO:
		dx12::Shader::PlatformParams* const shaderParams =
			dynamic_cast<dx12::Shader::PlatformParams*>(shader->GetPlatformParams());

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

			pipelineStateStream.rootSignature = m_rootSignature.Get();
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
		m_rootSignature = nullptr;
		m_pipelineState = nullptr;		
	}


	ID3D12PipelineState* PipelineState::GetD3DPipelineState() const
	{
		return m_pipelineState.Get();
	}


	ID3D12RootSignature* PipelineState::GetD3DRootSignature() const
	{
		return m_rootSignature.Get();
	}
}