// © 2023 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "RootSignature_DX12.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	RootSignature::RootSignature()
		: m_rootSignature(nullptr)
		, m_rootSigDescription{ 0 }
		, m_descriptorTableIdxBitmask(0)
	{
		// Zero our descriptor table entry counters:
		memset(m_numDescriptorsPerTable, 0, sizeof(m_numDescriptorsPerTable));

		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();
		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();


		// Create a root signature
		
		// TODO: Parse shaders using reflection and dynamically assemble a root signature
		// - Use shader introspection to bind structures by name to the correct registers
		// - Set shader visibility based on introspection search results?
		// For now, we just hack it so we can get something working. TODO: Integrate the working debug code properly
		


		// Describe our root signature layout:
		const uint8_t numRootParams = 1;
		CD3DX12_ROOT_PARAMETER1 rootParameters[numRootParams];



		// Debug: Insert a mat4 (i.e. 4x4 32-bit constants) directly into the root signature
		//rootParameters[0].InitAsConstants(
		//	sizeof(glm::mat4) / 4,				// num32BitValues
		//	0,									// shaderRegister
		//	0,									// registerSpace = 0
		//	D3D12_SHADER_VISIBILITY_VERTEX);	// shader visibility



		// Debug: Insert a CBV into the root signature
		//rootParameters[0].InitAsConstantBufferView(
		//	0, // shaderRegister
		//	0, // shader visibility
		//	D3D12_ROOT_DESCRIPTOR_FLAGS::D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
		//	D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL
		//);



		const uint8_t numDescriptorsInRange = 1;
		D3D12_DESCRIPTOR_RANGE1 descriptorRange;
		descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		descriptorRange.NumDescriptors = numDescriptorsInRange;
		descriptorRange.BaseShaderRegister = 0;
		descriptorRange.RegisterSpace = 0;
		descriptorRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAGS::D3D12_DESCRIPTOR_RANGE_FLAG_NONE; // ???
		descriptorRange.OffsetInDescriptorsFromTableStart = 0;

		rootParameters[0].InitAsDescriptorTable(
			1,									// UINT numDescriptorRanges
			&descriptorRange,					// const D3D12_DESCRIPTOR_RANGE1* pDescriptorRanges
			D3D12_SHADER_VISIBILITY_VERTEX);	// D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL
		
		// Pack the root sig bitmasks, so our GPUDescriptorHeap can parse the RootSignature
		// TODO: Populate these dynamically
		const uint8_t cbvRootIndex = 0;
		m_descriptorTableIdxBitmask = (1 << cbvRootIndex);
		m_numDescriptorsPerTable[cbvRootIndex] = 1;



		// Allow input layout and deny unnecessary access to certain pipeline stages
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		// Create the root signature description from our array of root parameters:
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription; // : D3D12_VERSIONED_ROOT_SIGNATURE_DESC: version + desc
		rootSignatureDescription.Init_1_1(
			_countof(rootParameters),	// Num parameters
			rootParameters,				// const D3D12_ROOT_PARAMETER1*
			0,							// Num static samplers
			nullptr,					// const D3D12_STATIC_SAMPLER_DESC*
			rootSignatureFlags);		// D3D12_ROOT_SIGNATURE_FLAGS

		// Cache the root signature description (we use D3D helper library for creation, but only care about caching
		// the native v1.1 D3D12 data structure)
		m_rootSigDescription = rootSignatureDescription.Desc_1_1;


		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		// Serialize the root signature:
		ComPtr<ID3DBlob> rootSignatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3DX12SerializeVersionedRootSignature(
			&rootSignatureDescription,
			featureData.HighestVersion,
			&rootSignatureBlob,
			&errorBlob);
		CheckHResult(hr, "Failed to serialize versioned root signature");

		// Create the root signature:
		hr = device->CreateRootSignature(
			0,
			rootSignatureBlob->GetBufferPointer(),
			rootSignatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&m_rootSignature));
		CheckHResult(hr, "Failed to create root signature");
	}


	RootSignature::~RootSignature()
	{
		Destroy();
	}


	void RootSignature::Destroy()
	{
		m_rootSignature = nullptr;
	}


	uint32_t RootSignature::GetDescriptorTableIdxBitmask() const
	{
		return m_descriptorTableIdxBitmask;
	}


	uint32_t RootSignature::GetNumDescriptors(uint8_t rootIndex) const
	{
		return m_numDescriptorsPerTable[rootIndex];
	}


	ID3D12RootSignature* RootSignature::GetD3DRootSignature() const
	{
		return m_rootSignature.Get();
	}


	D3D12_ROOT_SIGNATURE_DESC1 const& RootSignature::GetD3DRootSignatureDesc() const
	{
		return m_rootSigDescription;
	}
}