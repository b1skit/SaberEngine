// © 2023 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <d3dcompiler.h> // Supports SM 2 - 5.1.

#include "CastUtils.h"
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


	D3D12_DESCRIPTOR_RANGE_TYPE GetD3DRangeType(dx12::RootSignature::DescriptorType descType)
	{
		switch (descType)
		{
		case dx12::RootSignature::DescriptorType::SRV: return D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case dx12::RootSignature::DescriptorType::UAV: return D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		case dx12::RootSignature::DescriptorType::CBV: return D3D12_DESCRIPTOR_RANGE_TYPE::D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
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
		memset(&m_numDescriptorsPerTable, 0, sizeof(m_numDescriptorsPerTable));
	}


	RootSignature::~RootSignature()
	{
		Destroy();
	}


	void RootSignature::Destroy()
	{
		m_rootSignature = nullptr;

		// Zero our descriptor table entry counters:
		memset(&m_numDescriptorsPerTable, 0, sizeof(m_numDescriptorsPerTable));
		m_rootSigDescriptorTableIdxBitmask = 0;

		m_rootParams.clear();
		m_namesToRootParamsIdx.clear();

		m_descriptorTables.clear();
	}


	void RootSignature::InsertNewRootParamMetadata(char const* name, RootParameter&& rootParam)
	{
		SEAssert("RootParameter is not fully initialized", 
			rootParam.m_index != k_invalidRootSigIndex &&
			rootParam.m_type != RootParameter::Type::Type_Invalid &&
			rootParam.m_registerBindPoint != k_invalidRegisterVal &&
			rootParam.m_registerSpace != k_invalidRegisterVal);

		SEAssert("Constant union is not fully initialized", 
			rootParam.m_type != RootParameter::Type::Constant || 
				(rootParam.m_rootConstant.m_num32BitValues != k_invalidCount &&
				rootParam.m_rootConstant.m_destOffsetIn32BitValues != k_invalidOffset));

		SEAssert("Descriptor table union is not fully initialized",
			rootParam.m_type != RootParameter::Type::DescriptorTable || 
				(rootParam.m_tableEntry.m_type != DescriptorType::Type_Invalid &&
				rootParam.m_tableEntry.m_offset != k_invalidOffset));

		SEAssert("TODO: We currently assume all registers are specified in space 0. If this changes, we need to "
			"update our logic here to support lookups via register AND register space", 
			rootParam.m_registerSpace == 0);

		const size_t metadataIdx = m_rootParams.size();

		// Map the name to the insertion index:
		auto const& insertResult = m_namesToRootParamsIdx.emplace(name, metadataIdx);
		SEAssert("Name mapping metadata already exists", insertResult.second == true);

		// Map the register to the insertion index:
		DescriptorType insertType = DescriptorType::Type_Invalid;
		switch (rootParam.m_type)
		{
		case RootParameter::Type::Constant:
		case RootParameter::Type::CBV: // : register (b_, space_)
		{
			insertType = DescriptorType::CBV;
		}
		break;
		case RootParameter::Type::SRV: // : register (t_, space_)
		{
			insertType = DescriptorType::SRV;
		}
		break;
		case RootParameter::Type::UAV: // : register (u_, space_)
		{
			insertType = DescriptorType::UAV;
		}
		break;
		case RootParameter::Type::DescriptorTable:
		{
			switch (rootParam.m_tableEntry.m_type)
			{
			case DescriptorType::SRV:
			{
				insertType = DescriptorType::SRV;
			}
			break;
			case DescriptorType::UAV:
			{
				insertType = DescriptorType::UAV;
			}
			break;
			case DescriptorType::CBV:
			{
				insertType = DescriptorType::CBV;
			}
			break;
			default:
				SEAssertF("Invalid descriptor table type");
			}
		}
		break;
		default:
			SEAssertF("Invalid root parameter type");
		}

		auto const& registerToRootParamIdxInsertResult = 
			m_registerToRootParamIdx[insertType].emplace(rootParam.m_registerBindPoint, metadataIdx);
		SEAssert("Insertion index mapping metadata already exists", insertResult.second == true);

		// Finally, move the root param into our vector
		m_rootParams.emplace_back(std::move(rootParam));
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
		memset(&newRootSig->m_numDescriptorsPerTable, 0, sizeof(newRootSig->m_numDescriptorsPerTable));
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
		std::array<std::vector<RangeInput>, DescriptorType::Type_Count> rangeInputs;

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
			D3D12_SHADER_INPUT_BIND_DESC inputBindingDesc{};

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
					
					if (!newRootSig->m_namesToRootParamsIdx.contains(inputBindingDesc.Name))
					{
						const uint8_t rootIdx = util::CheckedCast<uint8_t>(rootParameters.size());
						rootParameters.emplace_back();

						rootParameters[rootIdx].InitAsConstantBufferView(
							inputBindingDesc.BindPoint,	// Shader register
							inputBindingDesc.Space,		// Register space
							D3D12_ROOT_DESCRIPTOR_FLAGS::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,	// Flags. TODO: Is volatile always appropriate?
							GetShaderVisibilityFlagFromShaderType(static_cast<dx12::Shader::ShaderType>(shaderIdx)));	// Shader visibility

						newRootSig->InsertNewRootParamMetadata(inputBindingDesc.Name,
							RootParameter{
								.m_index = rootIdx,
								.m_type = RootParameter::Type::CBV,
								.m_registerBindPoint = util::CheckedCast<uint8_t>(inputBindingDesc.BindPoint),
								.m_registerSpace = util::CheckedCast<uint8_t>(inputBindingDesc.Space)});

						// TODO: Test this
						SEAssert("TODO: Is this how we can tell if there is an array of CBVs? Need to test this",
							inputBindingDesc.BindCount == 1);
					}
					else
					{
						const size_t metadataIdx = newRootSig->m_namesToRootParamsIdx[inputBindingDesc.Name];
						rootParameters[newRootSig->m_rootParams[metadataIdx].m_index].ShaderVisibility =
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
						rangeInputs[DescriptorType::SRV].begin(),
						rangeInputs[DescriptorType::SRV].end(),
						RangeHasMatchingName);

					if (result == rangeInputs[DescriptorType::SRV].end())
					{
						rangeInputs[DescriptorType::SRV].emplace_back(
							RangeInput
							{
								.m_name = inputBindingDesc.Name,
								.m_baseRegister = util::CheckedCast<uint8_t>(inputBindingDesc.BindPoint),
								.m_registerSpace = util::CheckedCast<uint8_t>(inputBindingDesc.Space),
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
						rangeInputs[DescriptorType::UAV].begin(),
						rangeInputs[DescriptorType::UAV].end(),
						RangeHasMatchingName);

					if (result == rangeInputs[DescriptorType::UAV].end())
					{
						rangeInputs[DescriptorType::UAV].emplace_back(
							RangeInput
							{
								.m_name = inputBindingDesc.Name,
								.m_baseRegister = util::CheckedCast<uint8_t>(inputBindingDesc.BindPoint),
								.m_registerSpace = util::CheckedCast<uint8_t>(inputBindingDesc.Space),
								.m_shaderVisibility = GetShaderVisibilityFlagFromShaderType(
									static_cast<dx12::Shader::ShaderType>(shaderIdx)),
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
					if (!newRootSig->m_namesToRootParamsIdx.contains(inputBindingDesc.Name))
					{
						const uint8_t rootIdx = util::CheckedCast<uint8_t>(rootParameters.size());
						rootParameters.emplace_back();

						rootParameters[rootIdx].InitAsShaderResourceView(
							inputBindingDesc.BindPoint,	// Shader register
							inputBindingDesc.Space,		// Register space
							D3D12_ROOT_DESCRIPTOR_FLAGS::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,	// Flags. TODO: Is volatile always appropriate?
							GetShaderVisibilityFlagFromShaderType(static_cast<dx12::Shader::ShaderType>(shaderIdx)));	// Shader visibility

						newRootSig->InsertNewRootParamMetadata(inputBindingDesc.Name,
							RootParameter{
								.m_index = rootIdx,
								.m_type = RootParameter::Type::SRV,
								.m_registerBindPoint = util::CheckedCast<uint8_t>(inputBindingDesc.BindPoint),
								.m_registerSpace = util::CheckedCast<uint8_t>(inputBindingDesc.Space)});
					}
					else
					{
						const size_t metadataIdx = newRootSig->m_namesToRootParamsIdx[inputBindingDesc.Name];
						rootParameters[newRootSig->m_rootParams[metadataIdx].m_index].ShaderVisibility =
							D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL;
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
		tableRanges.resize(DescriptorType::Type_Count);

		for (size_t rangeTypeIdx = 0; rangeTypeIdx < DescriptorType::Type_Count; rangeTypeIdx++)
		{
			if (rangeInputs[rangeTypeIdx].size() == 0)
			{
				continue;
			}

			const DescriptorType rangeType = static_cast<DescriptorType>(rangeTypeIdx);

			// Sort the descriptors by register value, so they can be packed contiguously
			std::sort(
				rangeInputs[rangeTypeIdx].begin(),
				rangeInputs[rangeTypeIdx].end(),
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
			const uint8_t rootIdx = util::CheckedCast<uint8_t>(rootParameters.size());
			rootParameters.emplace_back();

			// Create a new descriptor table record, and populate the metadata as we go:
			newRootSig->m_descriptorTables.emplace_back();
			newRootSig->m_descriptorTables.back().m_index = rootIdx;
			
			// Walk through the sorted descriptors, and build ranges from contiguous blocks:
			size_t rangeStart = 0;
			size_t rangeEnd = 1;
			std::vector<std::string> namesInRange;
			D3D12_SHADER_VISIBILITY tableVisibility = rangeInputs[rangeTypeIdx][rangeStart].m_shaderVisibility;
			while (rangeStart < rangeInputs[rangeTypeIdx].size())
			{
				uint8_t expectedNextRegister = rangeInputs[rangeTypeIdx][rangeStart].m_baseRegister + 1;

				// Store the names in order so we can update the binding metadata later:
				namesInRange.emplace_back(rangeInputs[rangeTypeIdx][rangeStart].m_name);

				// Find the end of the current contiguous range:
				while (rangeEnd < rangeInputs[rangeTypeIdx].size() &&
					rangeInputs[rangeTypeIdx][rangeEnd].m_baseRegister == expectedNextRegister &&
					rangeInputs[rangeTypeIdx][rangeEnd].m_registerSpace == rangeInputs[rangeTypeIdx][rangeStart].m_registerSpace)
				{
					namesInRange.emplace_back(rangeInputs[rangeTypeIdx][rangeEnd].m_name);

					if (rangeInputs[rangeTypeIdx][rangeEnd].m_shaderVisibility != tableVisibility)
					{
						tableVisibility = D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL;
					}

					expectedNextRegister++;
					rangeEnd++;
				}

				// Initialize the descriptor range:
				const D3D12_DESCRIPTOR_RANGE_TYPE d3dRangeType = GetD3DRangeType(rangeType);
				
				const uint32_t numDescriptors = util::CheckedCast<uint32_t>(rangeEnd - rangeStart);
				tableRanges[rangeTypeIdx].emplace_back();

				const uint32_t baseRegister = rangeInputs[rangeTypeIdx][rangeStart].m_baseRegister;
				const uint32_t registerSpace = rangeInputs[rangeTypeIdx][rangeStart].m_registerSpace;		

				tableRanges[rangeTypeIdx].back().Init(
					d3dRangeType,
					numDescriptors,
					baseRegister,
					registerSpace,
					D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); //  TODO: Is this flag appropriate?

				// Populate the descriptor metadata:
				uint8_t baseRegisterOffset = 0; // We are processing contiguous ranges of registers only
				for (size_t rangeIdx = rangeStart; rangeIdx < rangeEnd; rangeIdx++)
				{
					// Populate the binding metadata for our individual descriptor table entries:
					newRootSig->InsertNewRootParamMetadata(
						namesInRange[rangeIdx].c_str(),
						RootParameter{
							.m_index = rootIdx,
							.m_type = RootParameter::Type::DescriptorTable,
							.m_registerBindPoint = util::CheckedCast<uint8_t>(baseRegister + baseRegisterOffset++),
							.m_registerSpace = util::CheckedCast<uint8_t>(registerSpace),
							.m_tableEntry = RootSignature::TableEntry{
								.m_type = rangeType,
								.m_offset = util::CheckedCast<uint8_t>(rangeIdx)}
						});

					// Populate the descriptor table metadata:
					switch (rangeType)
					{
					case DescriptorType::SRV:
					{
						const D3D12_SRV_DIMENSION d3d12SrvDimension =
							GetD3D12SRVDimension(rangeInputs[rangeTypeIdx][rangeIdx].m_dimension);

						RangeEntry newSrvRangeEntry;
						newSrvRangeEntry.m_srvDesc.m_format =
							GetFormatFromReturnType(rangeInputs[rangeTypeIdx][rangeIdx].m_returnType);
						newSrvRangeEntry.m_srvDesc.m_viewDimension = d3d12SrvDimension;

						newRootSig->m_descriptorTables.back().m_ranges[DescriptorType::SRV].emplace_back(newSrvRangeEntry);
					}
					break;
					case DescriptorType::UAV:
					{
						const D3D12_UAV_DIMENSION d3d12UavDimension =
							GetD3D12UAVDimension(rangeInputs[rangeTypeIdx][rangeIdx].m_dimension);

						RangeEntry newUavRangeEntry;
						newUavRangeEntry.m_uavDesc.m_format =
							GetFormatFromReturnType(rangeInputs[rangeTypeIdx][rangeIdx].m_returnType);
						newUavRangeEntry.m_uavDesc.m_viewDimension = d3d12UavDimension;

						newRootSig->m_descriptorTables.back().m_ranges[DescriptorType::UAV].emplace_back(newUavRangeEntry);
					}
					break;
					case DescriptorType::CBV:
					{
						SEAssertF("TODO: Handle this type");
					}
					break;
					default:
						SEAssertF("Invalid range type");
					}
				} // end rangeIdx loop

				// Prepare for the next iteration:
				rangeStart = rangeEnd;
				rangeEnd++;
			}

			// How many individual descriptor tables we're creating for the current range type:
			const uint32_t numDescriptorRanges = util::CheckedCast<uint32_t>(tableRanges[rangeTypeIdx].size());

			// Initialize the root parameter as a descriptor table built from our ranges:
			rootParameters[rootIdx].InitAsDescriptorTable(
				numDescriptorRanges,
				tableRanges[rangeTypeIdx].data(),
				tableVisibility);
		
			// How many descriptors are in the table stored at the given root sig index:
			newRootSig->m_numDescriptorsPerTable[rootIdx] = util::CheckedCast<uint32_t>(rangeInputs[rangeTypeIdx].size());

			const uint32_t descriptorTableBitmask = (1 << rootIdx);
			newRootSig->m_rootSigDescriptorTableIdxBitmask |= descriptorTableBitmask;

		} // End descriptor table DescriptorType loop


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
			util::CheckedCast<uint32_t>(rootParameters.size()),	// Num parameters
			rootParamsPtr,										// const D3D12_ROOT_PARAMETER1*
			util::CheckedCast<uint32_t>(staticSamplers.size()),	// Num static samplers
			staticSamplersPtr,									// const D3D12_STATIC_SAMPLER_DESC*
			rootSignatureFlags);								// D3D12_ROOT_SIGNATURE_FLAGS

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		// Before we create a root signature, check if one with the same layout already exists:
		const uint64_t rootSigDescHash = HashRootSigDesc(rootSignatureDescription);
		if (context->HasRootSignature(rootSigDescHash))
		{
			newRootSig = context->GetRootSignature(rootSigDescHash);
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
			ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

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
			context->AddRootSignature(newRootSig);
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
		auto const& result = m_namesToRootParamsIdx.find(resourceName);
		const bool hasResource = result != m_namesToRootParamsIdx.end();

		SEAssert("Root signature does not contain a parameter with that name", 
			hasResource || 
			en::Config::Get()->ValueExists(en::ConfigKeys::k_strictShaderBindingCmdLineArg) == false);

		return hasResource ? &m_rootParams[result->second] : nullptr;
	}


	RootSignature::RootParameter const* RootSignature::GetRootSignatureEntry(
		DescriptorType descriptorType, uint8_t registerBindPoint) const
	{
		SEAssert("Invalid descriptor type", descriptorType != DescriptorType::Type_Invalid);

		auto const& result = m_registerToRootParamIdx[descriptorType].find(registerBindPoint);
		const bool hasResource = result != m_registerToRootParamIdx[descriptorType].end();

		SEAssert("Root signature does not contain a parameter with that register/bind point",
			hasResource ||
			en::Config::Get()->ValueExists(en::ConfigKeys::k_strictShaderBindingCmdLineArg) == false);

		return hasResource ? &m_rootParams[result->second] : nullptr;
	}


	bool RootSignature::HasResource(std::string const& resourceName) const
	{
		return m_namesToRootParamsIdx.contains(resourceName);
	}
}