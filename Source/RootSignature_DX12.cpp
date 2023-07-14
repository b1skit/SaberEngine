// © 2023 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <d3dcompiler.h> // Supports SM 2 - 5.1.

#include "Context_DX12.h"
#include "Config.h"
#include "Debug_DX12.h"
#include "HashUtils.h"
#include "RootSignature_DX12.h"
#include "Sampler.h"
#include "Sampler_DX12.h"
#include "Shader.h"
#include "Shader_DX12.h"
#include "SysInfo_DX12.h"

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
		return D3D12_SHADER_VISIBILITY_ALL; // Return a reasonable default to suppress compiler warning
	}


	D3D12_DESCRIPTOR_RANGE_TYPE GetD3DRangeType(dx12::RootSignature::Range::Type rangeType)
	{
		switch (rangeType)
		{
		case dx12::RootSignature::Range::Type::SRV: return D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case dx12::RootSignature::Range::Type::UAV: return D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		case dx12::RootSignature::Range::Type::CBV: return D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		default:
			SEAssertF("Invalid range type");
		}
		return D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_CBV; // Suppress compiler warning
	}


	D3D12_SRV_DIMENSION GetD3D12SRVDimension(D3D_SRV_DIMENSION srvDimension)
	{
		// D3D_SRV_DIMENSION::D3D_SRV_DIMENSION_BUFFEREX (== 11, raw buffer resource) is handled differently in D3D12
		SEAssert("D3D_SRV_DIMENSION does not have a (known) D3D12_SRV_DIMENSION equivalent", 
			srvDimension >= D3D_SRV_DIMENSION_UNKNOWN && srvDimension <= D3D_SRV_DIMENSION_TEXTURECUBEARRAY);
		return static_cast<D3D12_SRV_DIMENSION>(srvDimension);
	}


	D3D12_UAV_DIMENSION GetD3D12UAVDimension(D3D_SRV_DIMENSION uavDimension)
	{
		SEAssert("D3D_SRV_DIMENSION does not have a (known) D3D12_UAV_DIMENSION equivalent",
			uavDimension >= D3D_SRV_DIMENSION_UNKNOWN && uavDimension <= D3D_SRV_DIMENSION_TEXTURE3D);
		return static_cast<D3D12_UAV_DIMENSION>(uavDimension);
	}


	DXGI_FORMAT GetFormatFromReturnType(D3D_RESOURCE_RETURN_TYPE returnType)
	{
		switch (returnType)
		{
			case D3D11_RETURN_TYPE_UNORM: return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
			case D3D11_RETURN_TYPE_SNORM: return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_SNORM;
			case D3D11_RETURN_TYPE_SINT: return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_SINT;
			case D3D11_RETURN_TYPE_UINT: return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UINT;
			case D3D11_RETURN_TYPE_FLOAT: return DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;
			case D3D11_RETURN_TYPE_MIXED:
			case D3D11_RETURN_TYPE_DOUBLE:
			case D3D11_RETURN_TYPE_CONTINUED:
			default:
				SEAssertF("Unexpected return type");
		}
		return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
	}


	uint64_t HashRootSigDesc(D3D12_VERSIONED_ROOT_SIGNATURE_DESC const& rootSigDesc)
	{
		uint64_t hash = 0;

		switch (rootSigDesc.Version)
		{
		case D3D_ROOT_SIGNATURE_VERSION::D3D_ROOT_SIGNATURE_VERSION_1_0:
		{
			SEAssertF("TODO: Support this");
		}
		break;
		case D3D_ROOT_SIGNATURE_VERSION::D3D_ROOT_SIGNATURE_VERSION_1_1:
		{
			// Parameters:
			util::AddDataToHash(hash, rootSigDesc.Desc_1_1.NumParameters);
			for (uint32_t paramIdx = 0; paramIdx < rootSigDesc.Desc_1_1.NumParameters; paramIdx++)
			{
				util::AddDataToHash(hash, rootSigDesc.Desc_1_1.pParameters[paramIdx].ParameterType);
				switch (rootSigDesc.Desc_1_1.pParameters[paramIdx].ParameterType)
				{
				case D3D12_ROOT_PARAMETER_TYPE::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
				{
					D3D12_ROOT_DESCRIPTOR_TABLE1 const& descriptorTable = 
						rootSigDesc.Desc_1_1.pParameters[paramIdx].DescriptorTable;
					for (uint32_t rangeIdx = 0; rangeIdx < descriptorTable.NumDescriptorRanges; rangeIdx++)
					{
						util::AddDataToHash(hash, descriptorTable.pDescriptorRanges[rangeIdx].RangeType);
						util::AddDataToHash(hash, descriptorTable.pDescriptorRanges[rangeIdx].NumDescriptors);
						util::AddDataToHash(hash, descriptorTable.pDescriptorRanges[rangeIdx].BaseShaderRegister);
						util::AddDataToHash(hash, descriptorTable.pDescriptorRanges[rangeIdx].RegisterSpace);
						util::AddDataToHash(hash, descriptorTable.pDescriptorRanges[rangeIdx].OffsetInDescriptorsFromTableStart);
					}
				}
				break;
				case D3D12_ROOT_PARAMETER_TYPE::D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
				{
					D3D12_ROOT_CONSTANTS const& rootConstant = rootSigDesc.Desc_1_1.pParameters[paramIdx].Constants;
					util::AddDataToHash(hash, rootConstant.ShaderRegister);
					util::AddDataToHash(hash, rootConstant.RegisterSpace);
					util::AddDataToHash(hash, rootConstant.Num32BitValues);
				}
				break;
				case D3D12_ROOT_PARAMETER_TYPE::D3D12_ROOT_PARAMETER_TYPE_CBV:
				case D3D12_ROOT_PARAMETER_TYPE::D3D12_ROOT_PARAMETER_TYPE_SRV:
				case D3D12_ROOT_PARAMETER_TYPE::D3D12_ROOT_PARAMETER_TYPE_UAV:
				{
					D3D12_ROOT_DESCRIPTOR1 const& rootDescriptor = rootSigDesc.Desc_1_1.pParameters[paramIdx].Descriptor;
					util::AddDataToHash(hash, rootDescriptor.ShaderRegister);
					util::AddDataToHash(hash, rootDescriptor.RegisterSpace);
					util::AddDataToHash(hash, rootDescriptor.Flags);
				}
				break;
				default:
					SEAssertF("Invalid parameter type");
				}

				util::AddDataToHash(hash, rootSigDesc.Desc_1_1.pParameters[paramIdx].ShaderVisibility);
			}

			// Samplers:
			util::AddDataToHash(hash, rootSigDesc.Desc_1_1.NumStaticSamplers);
			for (uint32_t samplerIdx = 0; samplerIdx < rootSigDesc.Desc_1_1.NumStaticSamplers; samplerIdx++)
			{
				D3D12_STATIC_SAMPLER_DESC const& samplerDesc = rootSigDesc.Desc_1_1.pStaticSamplers[samplerIdx];
				util::AddDataToHash(hash, samplerDesc.Filter);
				util::AddDataToHash(hash, samplerDesc.AddressU);
				util::AddDataToHash(hash, samplerDesc.AddressV);
				util::AddDataToHash(hash, samplerDesc.AddressW);

				// Hack: Interpret the float binary layout as a uint32_t
				util::AddDataToHash(hash, *reinterpret_cast<uint32_t const*>(&samplerDesc.MipLODBias));

				util::AddDataToHash(hash, samplerDesc.MaxAnisotropy);
				util::AddDataToHash(hash, samplerDesc.ComparisonFunc);
				util::AddDataToHash(hash, samplerDesc.BorderColor);

				util::AddDataToHash(hash, *reinterpret_cast<uint32_t const*>(&samplerDesc.MinLOD));
				util::AddDataToHash(hash, *reinterpret_cast<uint32_t const*>(&samplerDesc.MaxLOD));

				util::AddDataToHash(hash, samplerDesc.ShaderRegister);
				util::AddDataToHash(hash, samplerDesc.RegisterSpace);
				util::AddDataToHash(hash, samplerDesc.ShaderVisibility);
			}

			// Flags:
			util::AddDataToHash(hash, rootSigDesc.Desc_1_1.Flags);
		}
		break;
		default:
			SEAssertF("Invalid root signature version");
		}

		return hash;
	}
}

namespace dx12
{
	RootSignature::RootSignature()
		: m_rootSignature(nullptr)
		, m_rootSigDescHash(0)
		, m_rootSigDescriptorTableIdxBitmask(0)
	{
		memset(m_numDescriptorsPerTable, 0, sizeof(m_numDescriptorsPerTable));
	}


	RootSignature::~RootSignature()
	{
		Destroy();
	}


	void RootSignature::Destroy()
	{
		m_rootSignature = nullptr;

		// Zero our descriptor table entry counters:
		memset(m_numDescriptorsPerTable, 0, sizeof(m_numDescriptorsPerTable));
		m_rootSigDescriptorTableIdxBitmask = 0;

		m_namesToRootEntries.clear();
		m_descriptorTables.clear();
	}


	std::shared_ptr<dx12::RootSignature> RootSignature::Create(re::Shader const& shader)
	{
		// Note: We currently only support SM 5.1 here... TODO: Support SM 6+

		SEAssert("Shader must be created", shader.IsCreated());

		dx12::Shader::PlatformParams* shaderParams = shader.GetPlatformParams()->As<dx12::Shader::PlatformParams*>();

		SEAssert("No valid shader blobs found",
			shaderParams->m_shaderBlobs[dx12::Shader::ShaderType::Vertex] != nullptr ||
			shaderParams->m_shaderBlobs[dx12::Shader::ShaderType::Compute] != nullptr);

		std::shared_ptr<dx12::RootSignature> newRootSig = nullptr;
		newRootSig.reset(new dx12::RootSignature());

		// Zero our descriptor table entry counters: For each root sig. index containing a descriptor table, this tracks
		// how many descriptors are in that table
		memset(newRootSig->m_numDescriptorsPerTable, 0, sizeof(newRootSig->m_numDescriptorsPerTable));
		newRootSig->m_rootSigDescriptorTableIdxBitmask = 0;


		// We record details of descriptors we want to place into descriptor tables, and then build the tables later
		struct RangeInput // Mirrors D3D12_SHADER_INPUT_BIND_DESC
		{		
			std::string m_name;

			uint8_t m_baseRegister = k_invalidRegisterVal;	// BindPoint: Starting bind point
			uint8_t m_registerSpace = k_invalidRegisterVal; // Space: Register space
			
			D3D12_SHADER_VISIBILITY m_shaderVisibility = D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL;

			D3D_SHADER_INPUT_TYPE m_shaderInputType;	// Type: Type of resource (e.g. texture, cbuffer, etc.)
			uint32_t m_bindCount = 0;					// BindCount: Number of contiguous bind points (for arrays)
			D3D_RESOURCE_RETURN_TYPE m_returnType;		// ReturnType (Textures/UAVs)
			D3D_SRV_DIMENSION m_dimension;				// Dimension (Textures/UAVs)
			uint32_t m_numSamples = 0;					// NumSamples: Number of samples (0 if not MS texture)	
		};
		std::array<std::vector<RangeInput>, Range::Type::Type_Count> rangeInputs;

		constexpr size_t k_expectedNumberOfSamplers = 16; // Resource tier 1
		std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;
		staticSamplers.reserve(k_expectedNumberOfSamplers);

		std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters;
		rootParameters.reserve(k_totalRootSigDescriptorTableIndices);


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

			auto RangeHasMatchingName = [&inputBindingDesc](RangeInput const& a)
			{
				// TODO: We're currently assuming that all textures will be defined once in a common location, and
				// included in each different shader type (and thus will have the same index... but is this even
				// true?). This might not always be the case - We should fix this, it's brittle and stupid
				return strcmp(a.m_name.c_str(), inputBindingDesc.Name) == 0;
			};			

			for (uint32_t currentResource = 0; currentResource < shaderDesc.BoundResources; currentResource++)
			{
				hr = shaderReflection->GetResourceBindingDesc(currentResource, &inputBindingDesc);
				CheckHResult(hr, "Failed to get resource binding description");
				
				SEAssert("Too many root parameters. Consider increasing the root sig index type from a uint8_t", 
					rootParameters.size() < std::numeric_limits<uint8_t>::max());

				// Set the type-specific RootParameter values:
				switch (inputBindingDesc.Type)
				{
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_RTACCELERATIONSTRUCTURE:
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_FEEDBACKTEXTURE:
				{
					SEAssertF("TODO: Handle this resource type");
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D10_SIT_CBUFFER:
				{
					SEAssert("TODO: Handle root constants", strcmp(inputBindingDesc.Name, "$Globals") != 0);
					
					if (!newRootSig->m_namesToRootEntries.contains(inputBindingDesc.Name))
					{
						const uint8_t rootIdx = static_cast<uint8_t>(rootParameters.size());
						rootParameters.emplace_back();

						rootParameters[rootIdx].InitAsConstantBufferView(
							inputBindingDesc.BindPoint,	// Shader register
							inputBindingDesc.Space,		// Register space
							D3D12_ROOT_DESCRIPTOR_FLAGS::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,	// Flags. TODO: Is volatile always appropriate?
							GetShaderVisibilityFlagFromShaderType(static_cast<dx12::Shader::ShaderType>(shaderIdx)));	// Shader visibility

						newRootSig->m_namesToRootEntries.emplace(inputBindingDesc.Name,
							RootParameter{
								.m_index = rootIdx,
								.m_type = RootParameter::Type::CBV
							});

						// TODO: Test this
						SEAssert("TODO: Is this how we can tell if there is an array of CBVs? Need to test this",
							inputBindingDesc.BindCount == 1);
					}
					else
					{
						rootParameters[newRootSig->m_namesToRootEntries.at(inputBindingDesc.Name).m_index].ShaderVisibility =
							D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL;
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
					// Check to see if our texture has already been added (e.g. if it's referenced in multiple shader
					// stages). We do a linear search, but in practice the no. of elements is likely very small
					auto result = std::find_if(
						rangeInputs[Range::Type::SRV].begin(),
						rangeInputs[Range::Type::SRV].end(),
						RangeHasMatchingName);

					if (result == rangeInputs[Range::Type::SRV].end())
					{
						rangeInputs[Range::Type::SRV].emplace_back(
							RangeInput
							{
								.m_name = inputBindingDesc.Name,
								.m_baseRegister = static_cast<uint8_t>(inputBindingDesc.BindPoint),
								.m_registerSpace = static_cast<uint8_t>(inputBindingDesc.Space),
								.m_shaderVisibility =
									GetShaderVisibilityFlagFromShaderType(static_cast<dx12::Shader::ShaderType>(shaderIdx)),
								.m_shaderInputType = inputBindingDesc.Type,
								.m_bindCount = inputBindingDesc.BindCount,
								.m_returnType = inputBindingDesc.ReturnType,
								.m_dimension = inputBindingDesc.Dimension,
								.m_numSamples = inputBindingDesc.NumSamples
							});
					}
					else
					{
						result->m_shaderVisibility = D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL;
					}
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D10_SIT_SAMPLER:
				{
					std::shared_ptr<re::Sampler> sampler = re::Sampler::GetSampler(inputBindingDesc.Name);

					dx12::Sampler::PlatformParams* samplerPlatParams =
						sampler->GetPlatformParams()->As<dx12::Sampler::PlatformParams*>();

					auto HasSampler = [&inputBindingDesc, &samplerPlatParams](D3D12_STATIC_SAMPLER_DESC const& existing)
					{
						D3D12_STATIC_SAMPLER_DESC const& librarySampler = samplerPlatParams->m_staticSamplerDesc;
						return existing.Filter == librarySampler.Filter &&
							existing.AddressU == librarySampler.AddressU &&
							existing.AddressV == librarySampler.AddressV &&
							existing.AddressW == librarySampler.AddressW &&
							existing.MipLODBias == librarySampler.MipLODBias &&
							existing.MaxAnisotropy == librarySampler.MaxAnisotropy &&
							existing.ComparisonFunc == librarySampler.ComparisonFunc &&
							existing.BorderColor == librarySampler.BorderColor &&
							existing.MinLOD == librarySampler.MinLOD &&
							existing.MaxLOD == librarySampler.MaxLOD;
					};

					auto result = std::find_if(staticSamplers.begin(), staticSamplers.end(), HasSampler);
					if (result == staticSamplers.end())
					{
						staticSamplers.emplace_back(samplerPlatParams->m_staticSamplerDesc);
						staticSamplers.back().ShaderRegister = inputBindingDesc.BindPoint;
						staticSamplers.back().RegisterSpace = inputBindingDesc.Space;
						staticSamplers.back().ShaderVisibility =
							GetShaderVisibilityFlagFromShaderType(static_cast<dx12::Shader::ShaderType>(shaderIdx));
					}
					else
					{
						result->ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
					}
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D11_SIT_UAV_RWTYPED:
				{
					auto result = std::find_if(
						rangeInputs[Range::Type::UAV].begin(),
						rangeInputs[Range::Type::UAV].end(),
						RangeHasMatchingName);

					if (result == rangeInputs[Range::Type::UAV].end())
					{
						rangeInputs[Range::Type::UAV].emplace_back(
							RangeInput
							{
								.m_name = inputBindingDesc.Name,
								.m_baseRegister = static_cast<uint8_t>(inputBindingDesc.BindPoint),
								.m_registerSpace = static_cast<uint8_t>(inputBindingDesc.Space),
								.m_shaderVisibility =
									GetShaderVisibilityFlagFromShaderType(static_cast<dx12::Shader::ShaderType>(shaderIdx)),
								.m_shaderInputType = inputBindingDesc.Type,
								.m_bindCount = inputBindingDesc.BindCount,
								.m_returnType = inputBindingDesc.ReturnType,
								.m_dimension = inputBindingDesc.Dimension,
								.m_numSamples = inputBindingDesc.NumSamples
							});
					}
					else
					{
						SEAssert("Compute resource visibility should always be D3D12_SHADER_VISIBILITY_ALL",
							result->m_shaderVisibility == D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL);
					}
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D_SIT_STRUCTURED:
				{
					if (!newRootSig->m_namesToRootEntries.contains(inputBindingDesc.Name))
					{
						const uint8_t rootIdx = static_cast<uint8_t>(rootParameters.size());
						rootParameters.emplace_back();

						rootParameters[rootIdx].InitAsShaderResourceView(
							inputBindingDesc.BindPoint,	// Shader register
							inputBindingDesc.Space,		// Register space
							D3D12_ROOT_DESCRIPTOR_FLAGS::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,	// Flags. TODO: Is volatile always appropriate?
							GetShaderVisibilityFlagFromShaderType(static_cast<dx12::Shader::ShaderType>(shaderIdx)));	// Shader visibility

						newRootSig->m_namesToRootEntries.emplace(inputBindingDesc.Name,
							RootParameter{
								.m_index = rootIdx,
								.m_type = RootParameter::Type::SRV,
							}
						);
					}
					else
					{
						rootParameters[newRootSig->m_namesToRootEntries.at(inputBindingDesc.Name).m_index].ShaderVisibility =
							D3D12_SHADER_VISIBILITY_ALL;
					}
				}
				break;
				case D3D_SHADER_INPUT_TYPE::D3D11_SIT_UAV_RWSTRUCTURED:
				case D3D_SHADER_INPUT_TYPE::D3D11_SIT_BYTEADDRESS:
				case D3D_SHADER_INPUT_TYPE::D3D11_SIT_UAV_RWBYTEADDRESS:
				case D3D_SHADER_INPUT_TYPE::D3D11_SIT_UAV_APPEND_STRUCTURED:
				case D3D_SHADER_INPUT_TYPE::D3D11_SIT_UAV_CONSUME_STRUCTURED:
				case D3D_SHADER_INPUT_TYPE::D3D11_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
				{
					SEAssertF("TODO: Handle this resource type");
				}
				break;
				default:
					SEAssertF("Invalid resource type");
					continue;
				}	
			}
		}


		// TODO: Sort rootParameters based on the .ParameterType, to ensure optimal/preferred ordering/grouping of entries
		// -> MS recommends binding the most frequently changing elements at the start of the root signature.
		//		-> For SaberEngine, that's probably parameter blocks: CBVs and SRVs


		// Build our descriptor tables, and insert them into the root parameters.
		std::vector<std::vector<CD3DX12_DESCRIPTOR_RANGE1>> tableRanges;
		tableRanges.resize(Range::Type::Type_Count);

		for (size_t rangeType = 0; rangeType < Range::Type::Type_Count; rangeType++)
		{
			if (rangeInputs[rangeType].size() == 0)
			{
				continue;
			}

			// Sort the descriptors by register value, so they can be packed contiguously
			std::sort(
				rangeInputs[rangeType].begin(),
				rangeInputs[rangeType].end(),
				[](RangeInput const& a, RangeInput const& b)
				{
					if (a.m_baseRegister == b.m_baseRegister)
					{
						SEAssert("Register collision", a.m_registerSpace != b.m_registerSpace);
						return a.m_registerSpace < b.m_registerSpace;
					}
					return a.m_baseRegister < b.m_baseRegister;
				});

			// We're going to build a descriptor table entry at the current root index:
			const uint8_t rootIdx = static_cast<uint8_t>(rootParameters.size());
			rootParameters.emplace_back();

			// Create a new descriptor table record, and populate the metadata as we go:
			newRootSig->m_descriptorTables.emplace_back();
			newRootSig->m_descriptorTables.back().m_index = rootIdx;
			
			// Walk through the sorted descriptors, and build ranges from contiguous blocks:
			size_t rangeStart = 0;
			size_t rangeEnd = 1;
			std::vector<std::string> namesInRange;
			D3D12_SHADER_VISIBILITY tableVisibility = rangeInputs[rangeType][rangeStart].m_shaderVisibility;
			while (rangeStart < rangeInputs[rangeType].size())
			{
				uint8_t expectedNextRegister = rangeInputs[rangeType][rangeStart].m_baseRegister + 1;

				// Store the names in order so we can update the binding metadata later:
				namesInRange.emplace_back(rangeInputs[rangeType][rangeStart].m_name);

				// Find the end of the current contiguous range:
				while (rangeEnd < rangeInputs[rangeType].size() &&
					rangeInputs[rangeType][rangeEnd].m_baseRegister == expectedNextRegister &&
					rangeInputs[rangeType][rangeEnd].m_registerSpace == rangeInputs[rangeType][rangeStart].m_registerSpace)
				{
					namesInRange.emplace_back(rangeInputs[rangeType][rangeEnd].m_name);

					if (rangeInputs[rangeType][rangeEnd].m_shaderVisibility != tableVisibility)
					{
						tableVisibility = D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL;
					}

					expectedNextRegister++;
					rangeEnd++;
				}

				// Initialize the descriptor range:
				const uint32_t numDescriptorsInRange = static_cast<uint32_t>(rangeEnd - rangeStart);
				tableRanges[rangeType].emplace_back();

				tableRanges[rangeType].back().Init(
					GetD3DRangeType(static_cast<Range::Type>(rangeType)),
					numDescriptorsInRange,
					rangeInputs[rangeType][rangeStart].m_baseRegister,
					rangeInputs[rangeType][rangeStart].m_registerSpace,
					D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); //  TODO: Is this flag appropriate?

				// Populate the descriptor metadata:
				for (size_t rangeIdx = rangeStart; rangeIdx < rangeEnd; rangeIdx++)
				{
					switch (static_cast<Range::Type>(rangeType))
					{
					case Range::Type::SRV:
					{
						const D3D12_SRV_DIMENSION d3d12SrvDimension =
								GetD3D12SRVDimension(rangeInputs[rangeType][rangeIdx].m_dimension);

						RangeEntry newSrvRangeEntry;
						newSrvRangeEntry.m_srvDesc.m_format =
							GetFormatFromReturnType(rangeInputs[rangeType][rangeIdx].m_returnType);
						newSrvRangeEntry.m_srvDesc.m_viewDimension = d3d12SrvDimension;

						newRootSig->m_descriptorTables.back().m_ranges[Range::Type::SRV].emplace_back(newSrvRangeEntry);
					}
					break;
					case Range::Type::UAV:
					{
						const D3D12_UAV_DIMENSION d3d12UavDimension =
							GetD3D12UAVDimension(rangeInputs[rangeType][rangeIdx].m_dimension);

						RangeEntry newUavRangeEntry;
						newUavRangeEntry.m_uavDesc.m_format =
							GetFormatFromReturnType(rangeInputs[rangeType][rangeIdx].m_returnType);
						newUavRangeEntry.m_uavDesc.m_viewDimension = d3d12UavDimension;

						newRootSig->m_descriptorTables.back().m_ranges[Range::Type::UAV].emplace_back(newUavRangeEntry);
					}
					break;
					case Range::Type::CBV:
					{
						SEAssertF("TODO: Handle this type");
					}
					break;
					default:
						SEAssertF("Invalid range type");
					}
				}

				// Prepare for the next iteration:
				rangeStart = rangeEnd;
				rangeEnd++;
			}

			const uint32_t numDescriptorsInTable = static_cast<uint32_t>(tableRanges[rangeType].size());

			// Initialize the root parameter as a descriptor table built from our ranges:
			rootParameters[rootIdx].InitAsDescriptorTable(
				numDescriptorsInTable,
				tableRanges[rangeType].data(),
				tableVisibility);

			// Populate the binding metadata:
			for (uint8_t offset = 0; offset < namesInRange.size(); offset++)
			{
				newRootSig->m_namesToRootEntries.emplace(namesInRange[offset],
					RootParameter{
						.m_index = rootIdx,
						.m_type = RootParameter::Type::DescriptorTable,
						.m_tableEntry = RootSignature::TableEntry{ .m_offset = offset }
					});
			}

			// Mark the bitmasks. TODO: THE GPU DESCRIPTOR HEAP SHOULD PARSE THIS?
			newRootSig->m_numDescriptorsPerTable[rootIdx] = numDescriptorsInTable;

			const uint32_t descriptorTableBitmask = (1 << rootIdx);
			newRootSig->m_rootSigDescriptorTableIdxBitmask |= descriptorTableBitmask;

		} // End Range::Type loop

		// Allow input layout and deny unnecessary access to certain pipeline stages
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		// TODO: Dynamically choose the appropriate flags based on the shader stages seen during parsing
		// -> Set these at the beginning, and XOR them away if we encounter the specific shader types

		// Create the root signature description from our array of root parameters:
		D3D12_ROOT_PARAMETER1 const* rootParamsPtr = rootParameters.empty() ? nullptr : rootParameters.data();
		D3D12_STATIC_SAMPLER_DESC const* staticSamplersPtr = staticSamplers.empty() ? nullptr : staticSamplers.data();

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription; // : D3D12_VERSIONED_ROOT_SIGNATURE_DESC: version + desc
		rootSignatureDescription.Init_1_1(
			static_cast<uint32_t>(rootParameters.size()),	// Num parameters
			rootParamsPtr,									// const D3D12_ROOT_PARAMETER1*
			static_cast<uint32_t>(staticSamplers.size()),	// Num static samplers
			staticSamplersPtr,								// const D3D12_STATIC_SAMPLER_DESC*
			rootSignatureFlags);							// D3D12_ROOT_SIGNATURE_FLAGS

		// Before we create a root signature, check if one with the same layout already exists:
		const uint64_t rootSigDescHash = HashRootSigDesc(rootSignatureDescription);
		if (dx12::Context::HasRootSignature(rootSigDescHash))
		{
			newRootSig = dx12::Context::GetRootSignature(rootSigDescHash);

			SEAssertF("This is not an error: Just a test to ensure the root signature library is functioning correctly."
				" If you hit this assert and have 2 shaders with the same root signature it is good news! Delete it, "
				"give yourself a high-five, and move on");
		}
		else
		{
			newRootSig->m_rootSigDescHash = rootSigDescHash;

			// Serialize the root signature:
			ComPtr<ID3DBlob> rootSignatureBlob = nullptr;
			ComPtr<ID3DBlob> errorBlob = nullptr;
			HRESULT hr = D3DX12SerializeVersionedRootSignature(
				&rootSignatureDescription,
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
				IID_PPV_ARGS(&newRootSig->m_rootSignature));
			CheckHResult(hr, "Failed to create root signature");

			const std::wstring rootSigName = shader.GetWName() + L"_RootSig";
			newRootSig->m_rootSignature->SetName(rootSigName.c_str());

			// Add the new root sig to the library:
			dx12::Context::AddRootSignature(newRootSig);
		}

		return newRootSig;
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


	RootSignature::RootParameter const* RootSignature::GetRootSignatureEntry(std::string const& resourceName) const
	{
		auto const& result = m_namesToRootEntries.find(resourceName);
		const bool hasResource = result != m_namesToRootEntries.end();

		SEAssert("Root signature does not contain a parameter with that name", 
			hasResource || 
			en::Config::Get()->ValueExists(en::Config::k_relaxedShaderBindingCmdLineArg) == true);

		return hasResource ? &result->second : nullptr;
	}


	bool RootSignature::HasResource(std::string const& resourceName) const
	{
		return m_namesToRootEntries.contains(resourceName);
	}
}