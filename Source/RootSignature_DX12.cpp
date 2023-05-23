// © 2023 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <d3dcompiler.h> // Supports SM 2 - 5.1.

#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "SysInfo_DX12.h"
#include "RootSignature_DX12.h"
#include "Sampler.h"
#include "Sampler_DX12.h"
#include "Shader.h"
#include "Shader_DX12.h"

using Microsoft::WRL::ComPtr;


namespace
{
	constexpr uint8_t k_invalidIdx = std::numeric_limits<uint8_t>::max();
}

namespace dx12
{
	RootSignature::RootSignature()
		: m_rootSignature(nullptr)
		, m_rootSigDescription{}
		, m_descriptorTableIdxBitmask(0)
	{
	}


	RootSignature::~RootSignature()
	{
		Destroy();
	}


	void RootSignature::Destroy()
	{
		m_rootSignature = nullptr;
		m_rootSigDescription = {};

		m_descriptorTableIdxBitmask = 0;

		// Zero our descriptor table entry counters:
		memset(m_numDescriptorsPerTableEntry, 0, sizeof(m_numDescriptorsPerTableEntry));

		m_namesToRootEntries.clear();
	}


	void RootSignature::Create(re::Shader const& shader)
	{
		// Note: We currently only support SM 5.1 here... TODO: Support SM 6+

		SEAssert("Shader must be created", shader.IsCreated());

		dx12::Shader::PlatformParams* shaderParams = shader.GetPlatformParams()->As<dx12::Shader::PlatformParams*>();

		SEAssert("No vertex shader found. TODO: Support root signatures for pipeline configurations without a vertex "
			"shader (e.g. compute)",
			shaderParams->m_shaderBlobs[dx12::Shader::ShaderType::Vertex] != nullptr);

		// Zero our descriptor table entry counters: For each root sig. index containing a descriptor table, this tracks
		// how many descriptors are in that table
		memset(m_numDescriptorsPerTableEntry, 0, sizeof(m_numDescriptorsPerTableEntry));

		/* Root signature layout: We tightly pack entries in the following order:
		* Constant buffer root descriptors (CBV)
		* Structured buffer root descriptors (SRV)
		* SRV descriptor tables
		* UAV descriptor tables
		* Sampler descriptor tables
		*
		* Note: MS recommends binding the most frequently changing elements at the start of the root signature.
		* For SaberEngine, that's probably parameter blocks: CBVs and SRVs
		*/
		
		uint32_t numCBVs = 0; // D3D12_SHADER_DESC::ConstantBuffers is per-stage; We count the total across all stages
		uint32_t numSRVs = 0;
		// TODO: Count more resource types

		// Parse the shader reflection:
		ComPtr<ID3D12ShaderReflection> shaderReflection;
		for (uint32_t shaderIdx = 0; shaderIdx < Shader::ShaderType::ShaderType_Count; shaderIdx++)
		{
			if (shaderParams->m_shaderBlobs[shaderIdx] == nullptr)
			{
				continue;
			}

			HRESULT hr = D3DReflect(
				shaderParams->m_shaderBlobs[shaderIdx]->GetBufferPointer(),
				shaderParams->m_shaderBlobs[shaderIdx]->GetBufferSize(),
				IID_PPV_ARGS(&shaderReflection)
			);
			CheckHResult(hr, "Failed to reflect shader");

			// Get a description of the entire shader:
			D3D12_SHADER_DESC shaderDesc{};
			hr = shaderReflection->GetDesc(&shaderDesc);
			CheckHResult(hr, "Failed to get shader description");

			// Parse the resource bindings for the current shader stage:
			D3D12_SHADER_INPUT_BIND_DESC inputBindingDesc;
			for (uint32_t currentResource = 0; currentResource < shaderDesc.BoundResources; currentResource++)
			{
				hr = shaderReflection->GetResourceBindingDesc(currentResource, &inputBindingDesc);
				CheckHResult(hr, "Failed to get resource binding description");

				RootSigEntry rootEntry{};
				rootEntry.m_baseRegister = inputBindingDesc.BindPoint;
				rootEntry.m_registerSpace = inputBindingDesc.Space;

				// Fix these up later once we've counted everything
				rootEntry.m_type = EntryType::EntryType_Invalid;
				rootEntry.m_rootSigIndex = k_invalidIdx;
				rootEntry.m_offset = 0;
				rootEntry.m_count = 1;
				
				// Shader visibility:
				switch (shaderIdx)
				{
				case Shader::ShaderType::Vertex:
				{
					rootEntry.m_shaderVisibility = D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_VERTEX;
				}
				break;
				case Shader::ShaderType::Geometry:
				{
					rootEntry.m_shaderVisibility = D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_GEOMETRY;
				}
				break;
				case Shader::ShaderType::Pixel:
				{
					rootEntry.m_shaderVisibility = D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_PIXEL;
				}
				break;
				case Shader::ShaderType::Compute:
				{
					// Compute queue always uses D3D12_SHADER_VISIBILITY_ALL because it has only 1 active stage
					rootEntry.m_shaderVisibility = D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL;
				}
				break;
				default:
					SEAssertF("Invalid shader type");
				}


				// Parse the current binding description:
				switch (inputBindingDesc.Type)
				{
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_RTACCELERATIONSTRUCTURE:
				{
					SEAssertF("TODO: Handle this resource type");
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_FEEDBACKTEXTURE:
				{
					SEAssertF("TODO: Handle this resource type");
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D10_SIT_CBUFFER:
				{
					SEAssert("TODO: Handle root constants", 
						strcmp(inputBindingDesc.Name, "$Globals") != 0);
					
					rootEntry.m_type = EntryType::RootCBV;					
					if (m_namesToRootEntries.contains(inputBindingDesc.Name) == false)
					{
						numCBVs++; // Only increment if we're about to insert a new entry
					}
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D10_SIT_TBUFFER:
				{
					SEAssertF("TODO: Handle this resource type");
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE:
				{
					SEAssertF("TODO: Handle this resource type");
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D10_SIT_SAMPLER:
				{
					SEAssertF("TODO: Handle this resource type");
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D11_SIT_UAV_RWTYPED:
				{
					SEAssertF("TODO: Handle this resource type");
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_STRUCTURED:
				{
					rootEntry.m_type = EntryType::RootSRV;
					if (m_namesToRootEntries.contains(inputBindingDesc.Name) == false)
					{
						numSRVs++; // Only increment if we're about to insert a new entry
					}
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D11_SIT_UAV_RWSTRUCTURED:
				{
					SEAssertF("TODO: Handle this resource type");
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D11_SIT_BYTEADDRESS:
				{
					SEAssertF("TODO: Handle this resource type");
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D11_SIT_UAV_RWBYTEADDRESS:
				{
					SEAssertF("TODO: Handle this resource type");
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D11_SIT_UAV_APPEND_STRUCTURED:
				{
					SEAssertF("TODO: Handle this resource type");
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D11_SIT_UAV_CONSUME_STRUCTURED:
				{
					SEAssertF("TODO: Handle this resource type");
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D11_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
				{
					SEAssertF("TODO: Handle this resource type");
				}
				break;
				default:
					SEAssertF("Invalid resource type");
					continue;
				}

				// Record the entry:
				auto const& insertResult =
					m_namesToRootEntries.insert({ inputBindingDesc.Name, rootEntry });

				// If the element already exists, update the visibility flag
				// TODO: Handle this in a more sophisticated way than just falling back to global visiblity
				if (insertResult.second == false)
				{
					insertResult.first->second.m_shaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				}
			}
		}

		const uint32_t totalUniqueShaderResources = static_cast<uint32_t>(m_namesToRootEntries.size());

		SEAssert("The maximum size of a root signature is 64 DWORDS. We currently assume we'll use 32 or less "
			"(i.e. a descriptor table in every entry) in the worst case. If we hit this assert, we'll need to "
			"implement some more sophisticated logic for laying out the entries here",
			totalUniqueShaderResources <= 32);

		// Build the root signature layout (for all Shader stages):
		std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters;
		rootParameters.resize(totalUniqueShaderResources);

		uint8_t currentCBVIdx = 0;
		uint8_t currentSRVIdx = numCBVs; // Offset

		for (auto& rootEntry : m_namesToRootEntries)
		{
			switch (rootEntry.second.m_type)
			{
			case EntryType::RootConstant:
			{
				SEAssertF("TODO: Handle root constants");
			}
			break;
			case EntryType::RootCBV:
			{
				rootEntry.second.m_rootSigIndex = currentCBVIdx++;

				// We (currently) insert CBVs directly into the root signature:
				rootParameters[rootEntry.second.m_rootSigIndex].InitAsConstantBufferView(
					rootEntry.second.m_baseRegister,										// Shader register
					rootEntry.second.m_registerSpace,										// Register space
					D3D12_ROOT_DESCRIPTOR_FLAGS::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,	// Flags. TODO: Is volatile always appropriate?
					rootEntry.second.m_shaderVisibility);									// Shader visibility
			}
			break;
			case EntryType::RootSRV:
			{
				rootEntry.second.m_rootSigIndex = currentSRVIdx++;

				rootParameters[rootEntry.second.m_rootSigIndex].InitAsShaderResourceView(
					rootEntry.second.m_baseRegister,										// Shader register
					rootEntry.second.m_registerSpace,										// Register space
					D3D12_ROOT_DESCRIPTOR_FLAGS::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,	// Flags. TODO: Is volatile always appropriate?
					rootEntry.second.m_shaderVisibility);									// Shader visibility
			}
			break;
			case EntryType::DescriptorTable:
			{
				SEAssertF("TODO: Handle descriptor tables");

				//// Set the root sig descriptor table bitmasks, so our CommandLists's GPUDescriptorHeap can parse the
				//// RootSignature later on
				//m_descriptorTableIdxBitmask = (1 << rootEntry.second.m_rootSigIndex);
				//m_numDescriptorsPerTableEntry[rootEntry.second.m_rootSigIndex] = 1; // TODO: Support arrays/unbounded arrays
			}
			break;
			case EntryType::Sampler:
			{
				SEAssertF("TODO: Handle this resource type");
			}
			break;
			case EntryType::StaticSampler:
			{
				SEAssertF("TODO: Handle this resource type");
			}
			break;
			default:
				SEAssertF("Invalid type");
			}
		}


		// TODO: Figure out how to populate this during parsing
		uint32_t numStaticSamplers = 0;
		std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;
		staticSamplers.reserve(numStaticSamplers);


		// Allow input layout and deny unnecessary access to certain pipeline stages
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		// TODO: Dynamically choose the appropriate flags

		// Create the root signature description from our array of root parameters:
		D3D12_ROOT_PARAMETER1 const* rootParamsPtr = rootParameters.empty() ? nullptr : &rootParameters[0];
		D3D12_STATIC_SAMPLER_DESC const* staticSamplersPtr = staticSamplers.empty() ? nullptr : &staticSamplers[0];

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription; // : D3D12_VERSIONED_ROOT_SIGNATURE_DESC: version + desc
		rootSignatureDescription.Init_1_1(
			static_cast<uint32_t>(rootParameters.size()),	// Num parameters
			rootParamsPtr,									// const D3D12_ROOT_PARAMETER1*
			numStaticSamplers,								// Num static samplers
			staticSamplersPtr,								// const D3D12_STATIC_SAMPLER_DESC*
			rootSignatureFlags);							// D3D12_ROOT_SIGNATURE_FLAGS

		// Cache the root signature description so it can be parsed by the GPUDescriptorHeap
		m_rootSigDescription = rootSignatureDescription;

		const D3D_ROOT_SIGNATURE_VERSION rootSigVersion = SysInfo::GetHighestSupportedRootSignatureVersion();

		// Serialize the root signature:
		ComPtr<ID3DBlob> rootSignatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3DX12SerializeVersionedRootSignature(
			&m_rootSigDescription,
			rootSigVersion,
			&rootSignatureBlob,
			&errorBlob);
		CheckHResult(hr, "Failed to serialize versioned root signature");


		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();
		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		// Create the root signature:
		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs
		hr = device->CreateRootSignature(
			deviceNodeMask,
			rootSignatureBlob->GetBufferPointer(),
			rootSignatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&m_rootSignature));
		CheckHResult(hr, "Failed to create root signature");
	}


	uint32_t RootSignature::GetDescriptorTableIdxBitmask() const
	{
		return m_descriptorTableIdxBitmask;
	}


	uint32_t RootSignature::GetNumDescriptorsInTable(uint8_t rootIndex) const
	{
		return m_numDescriptorsPerTableEntry[rootIndex];
	}


	ID3D12RootSignature* RootSignature::GetD3DRootSignature() const
	{
		return m_rootSignature.Get();
	}


	D3D12_ROOT_SIGNATURE_DESC1 const& RootSignature::GetD3DRootSignatureDesc() const
	{
		// TODO: We should returned the versioned one, and handle that at the caller
		#pragma message("TODO: Support variable root sig versions in RootSignature::GetD3DRootSignatureDesc")
		return m_rootSigDescription.Desc_1_1;
	}


	RootSignature::RootSigEntry const& RootSignature::GetResourceRegisterBindPoint(std::string const& resourceName) const
	{
		auto const& result = m_namesToRootEntries.find(resourceName);
		SEAssert("Root signature does not contain a parameter with that name", result != m_namesToRootEntries.end());
		
		return result->second;
	}
}