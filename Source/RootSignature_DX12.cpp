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


	D3D12_SHADER_VISIBILITY GetShaderVisibilityFlagFromShaderType(dx12::Shader::ShaderType shaderType)
	{
		switch (shaderType)
		{
		case dx12::Shader::ShaderType::Vertex:		return D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_VERTEX;
		case dx12::Shader::ShaderType::Geometry:	return D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_GEOMETRY;
		case dx12::Shader::ShaderType::Pixel:		return D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_PIXEL;
		case dx12::Shader::ShaderType::Compute:		return D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL;
							// Compute queue always uses D3D12_SHADER_VISIBILITY_ALL because it has only 1 active stage
		default:
			SEAssertF("Invalid shader type");
		}
	}
}

namespace dx12
{
	RootSignature::RootSignature()
		: m_rootSignature(nullptr)
		, m_rootSigDescription{}
		, m_rootSigDescriptorTableIdxBitmask(0)
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

		m_rootSigDescriptorTableIdxBitmask = 0;

		// Zero our descriptor table entry counters:
		memset(m_numDescriptorsPerTable, 0, sizeof(m_numDescriptorsPerTable));

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
		memset(m_numDescriptorsPerTable, 0, sizeof(m_numDescriptorsPerTable));

		/* Root signature layout: We tightly pack entries in the following order:
		* Constant buffer root descriptors (CBV)
		* Structured buffer root descriptors (SRV)
		* Texture SRV descriptor tables
		* UAV descriptor tables
		* Sampler descriptor tables
		*
		* Note: MS recommends binding the most frequently changing elements at the start of the root signature.
		* For SaberEngine, that's probably parameter blocks: CBVs and SRVs
		*/
		
		uint32_t numCBVs = 0; // D3D12_SHADER_DESC::ConstantBuffers is per-stage; We count the total across all stages
		uint32_t numRootSRVs = 0;
		uint32_t numTextureSRVs = 0;
		uint32_t numSamplers = 0;
		// TODO: Count more resource types

		std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;		

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
				rootEntry.m_shaderVisibility = 
					GetShaderVisibilityFlagFromShaderType(static_cast<dx12::Shader::ShaderType>(shaderIdx));
				
				// Set the type-specific RootSigEntry values:
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
					SEAssert("TODO: Handle root constants", strcmp(inputBindingDesc.Name, "$Globals") != 0);
					
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
					rootEntry.m_type = EntryType::TextureSRV;
					if (m_namesToRootEntries.contains(inputBindingDesc.Name) == false)
					{
						rootEntry.m_offset = numTextureSRVs;
						numTextureSRVs++; // Only increment if we're about to insert a new entry
					}
					else
					{
						rootEntry.m_offset = m_namesToRootEntries.at(inputBindingDesc.Name).m_offset;
					}
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D10_SIT_SAMPLER:
				{
					rootEntry.m_type = EntryType::Sampler;
					if (m_namesToRootEntries.contains(inputBindingDesc.Name) == false)
					{
						numSamplers++; // Only increment if we're about to insert a new entry

						std::shared_ptr<re::Sampler> sampler = re::Sampler::GetSampler(inputBindingDesc.Name);

						dx12::Sampler::PlatformParams* samplerPlatParams =
							sampler->GetPlatformParams()->As<dx12::Sampler::PlatformParams*>();

						// Copy the pre-initialzed sampler description, and populate the remaining values:
						staticSamplers.emplace_back(samplerPlatParams->m_staticSamplerDesc);
						staticSamplers.back().ShaderRegister = rootEntry.m_baseRegister;
						staticSamplers.back().RegisterSpace = rootEntry.m_registerSpace;
						staticSamplers.back().ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
					}	
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
						numRootSRVs++; // Only increment if we're about to insert a new entry
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
				SEAssert("Some root entries have not been initialized", 
					rootEntry.m_type != EntryType::EntryType_Invalid &&
					rootEntry.m_baseRegister != k_invalidRegisterVal &&
					rootEntry.m_registerSpace != k_invalidRegisterVal);
				SEAssert("Some root entries have been prematurely modified", 
					rootEntry.m_rootSigIndex == k_invalidRootSigIndex && 
					(rootEntry.m_offset == k_invalidOffset || rootEntry.m_type == EntryType::TextureSRV)&&
					rootEntry.m_count == 0);

				// Record the entry:
				auto const& insertResult = m_namesToRootEntries.insert({ inputBindingDesc.Name, rootEntry });

				// If the element already exists, make it always visible
				if (insertResult.second == false)
				{
					insertResult.first->second.m_shaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				}
			}
		}

		// Count the number of root parameters:
		// - Textures are (currently) combined into a single descriptor table
		// - We don't include samplers
		const uint32_t numTextureSRVDescriptorTables = numTextureSRVs > 0 ? 1 : 0; // TODO: Will we ever want > 1 here?
		const uint32_t totalNumRootParams = numCBVs + numRootSRVs + numTextureSRVDescriptorTables;

		SEAssert("The maximum size of a root signature is 64 DWORDS. We currently assume we'll use 32 or less "
			"(i.e. a descriptor table in every entry) in the worst case. If we hit this assert, we'll need to "
			"implement some more sophisticated logic for laying out the entries here",
			totalNumRootParams <= 32);

		// Build the root signature layout (for all Shader stages):
		std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters;
		rootParameters.resize(totalNumRootParams);

		uint8_t currentCBVIdx = 0;
		uint8_t currentSRVIdx = numCBVs; // Offset
		uint8_t currentTextureSRVDescriptorTableIdx = currentSRVIdx + numRootSRVs; // Offset

		// TODO: We could combine descriptors tables into an array of packed range entries (CBVs, SRVs, etc)
		CD3DX12_DESCRIPTOR_RANGE1 textureSRVDescriptorRange;

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
				rootEntry.second.m_offset = 0;
				rootEntry.second.m_count = 1;

				// We (currently) insert CBVs directly into the root signature:
				rootParameters[rootEntry.second.m_rootSigIndex].InitAsConstantBufferView(
					rootEntry.second.m_baseRegister,										// Shader register
					rootEntry.second.m_registerSpace,										// Register space
					D3D12_ROOT_DESCRIPTOR_FLAGS::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,	// Flags. TODO: Is volatile always appropriate?
					rootEntry.second.m_shaderVisibility);									// Shader visibility

				// NOTE: dx12::CommandList::SetParameterBlock will need to be updated when we solve the PB CBV/SRV issue
			}
			break;
			case EntryType::RootSRV:
			{
				rootEntry.second.m_rootSigIndex = currentSRVIdx++;
				rootEntry.second.m_offset = 0;
				rootEntry.second.m_count = 1;

				rootParameters[rootEntry.second.m_rootSigIndex].InitAsShaderResourceView(
					rootEntry.second.m_baseRegister,										// Shader register
					rootEntry.second.m_registerSpace,										// Register space
					D3D12_ROOT_DESCRIPTOR_FLAGS::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,	// Flags. TODO: Is volatile always appropriate?
					rootEntry.second.m_shaderVisibility);									// Shader visibility

				// NOTE: dx12::CommandList::SetParameterBlock will need to be updated when we solve the PB CBV/SRV issue
			}
			break;
			case EntryType::TextureSRV:
			{
				// Root sig index will be the same for all entries in the descriptor table
				rootEntry.second.m_rootSigIndex = currentTextureSRVDescriptorTableIdx;
				SEAssert("Offset has not been set", rootEntry.second.m_offset != k_invalidOffset);
				rootEntry.second.m_count = numTextureSRVs; // Root entries in the same table have the same count

				// If this is the first time we've seen a texture SRV descriptor table, initialize our descriptor range
				const uint32_t textureSRVDescriptorTableIdxBitmask = (1 << rootEntry.second.m_rootSigIndex);
				if ((m_rootSigDescriptorTableIdxBitmask & textureSRVDescriptorTableIdxBitmask) == 0)
				{
					// Set the root sig descriptor table bitmasks, so our CommandLists's GPUDescriptorHeap can parse the
					// RootSignature later on
					m_rootSigDescriptorTableIdxBitmask |= textureSRVDescriptorTableIdxBitmask;
					m_numDescriptorsPerTable[rootEntry.second.m_rootSigIndex] = numTextureSRVs;

					// Initialize the descriptor range:
					textureSRVDescriptorRange.Init(
						D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
						numTextureSRVs,
						rootEntry.second.m_baseRegister,
						rootEntry.second.m_registerSpace,
						D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); //  TODO: Is this flag appropriate?

					rootParameters[currentTextureSRVDescriptorTableIdx].InitAsDescriptorTable(
						1,
						&textureSRVDescriptorRange,
						rootEntry.second.m_shaderVisibility);					
				}
				else
				{
					SEAssert("Descriptor range should have been initialized with the lowest register & same space", 
						textureSRVDescriptorRange.BaseShaderRegister < rootEntry.second.m_baseRegister &&
						textureSRVDescriptorRange.RegisterSpace == rootEntry.second.m_registerSpace);
				}
				SEAssert("Unexpected descriptor table entry tracker value", 
					m_numDescriptorsPerTable[rootEntry.second.m_rootSigIndex] == numTextureSRVs);
			}
			break;
			case EntryType::Sampler:
			{
				continue; // staticSamplers vector has already been fully popualted
			}
			break;
			default:
				SEAssertF("Invalid type");
			}

			SEAssert("Some root entries are not initilized",
				rootEntry.second.m_rootSigIndex != k_invalidRootSigIndex &&
				rootEntry.second.m_offset != k_invalidOffset &&
				rootEntry.second.m_count != 0);
		}

		// Allow input layout and deny unnecessary access to certain pipeline stages
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		// TODO: Dynamically choose the appropriate flags based on the shader stages seen during parsing

		// Create the root signature description from our array of root parameters:
		D3D12_ROOT_PARAMETER1 const* rootParamsPtr = rootParameters.empty() ? nullptr : &rootParameters[0];
		D3D12_STATIC_SAMPLER_DESC const* staticSamplersPtr = staticSamplers.empty() ? nullptr : &staticSamplers[0];

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription; // : D3D12_VERSIONED_ROOT_SIGNATURE_DESC: version + desc
		rootSignatureDescription.Init_1_1(
			static_cast<uint32_t>(rootParameters.size()),	// Num parameters
			rootParamsPtr,									// const D3D12_ROOT_PARAMETER1*
			static_cast<uint32_t>(staticSamplers.size()),	// Num static samplers
			staticSamplersPtr,								// const D3D12_STATIC_SAMPLER_DESC*
			rootSignatureFlags);							// D3D12_ROOT_SIGNATURE_FLAGS

		// Cache the root signature description so it can be parsed by the GPUDescriptorHeap
		m_rootSigDescription = rootSignatureDescription;

		// Serialize the root signature:
		ComPtr<ID3DBlob> rootSignatureBlob = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3DX12SerializeVersionedRootSignature(
			&m_rootSigDescription,
			SysInfo::GetHighestSupportedRootSignatureVersion(),
			&rootSignatureBlob,
			&errorBlob);
		CheckHResult(hr, "Failed to serialize versioned root signature");


		// Create the root signature:
		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();
		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs
		hr = device->CreateRootSignature(
			deviceNodeMask,
			rootSignatureBlob->GetBufferPointer(),
			rootSignatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&m_rootSignature));
		CheckHResult(hr, "Failed to create root signature");

		const std::wstring rootSigName = shader.GetWName() + L"_RootSig";
		m_rootSignature->SetName(rootSigName.c_str());
	}


	uint32_t RootSignature::GetDescriptorTableIdxBitmask() const
	{
		return m_rootSigDescriptorTableIdxBitmask;
	}


	uint32_t RootSignature::GetNumDescriptorsInTable(uint8_t rootIndex) const
	{
		return m_numDescriptorsPerTable[rootIndex];
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


	RootSignature::RootSigEntry const& RootSignature::GetRootSignatureEntry(std::string const& resourceName) const
	{
		auto const& result = m_namesToRootEntries.find(resourceName);
		SEAssert("Root signature does not contain a parameter with that name", result != m_namesToRootEntries.end());

		return result->second;
	}


	bool RootSignature::HasResource(std::string const& resourceName) const
	{
		return m_namesToRootEntries.contains(resourceName);
	}
}