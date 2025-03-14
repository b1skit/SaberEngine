// © 2023 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "RootSignature_DX12.h"
#include "Sampler.h"
#include "Sampler_DX12.h"
#include "Shader.h"
#include "Shader_DX12.h"
#include "SysInfo_DX12.h"

#include "Core/Assert.h"
#include "Core/Config.h"

#include "Core/Util/CastUtils.h"
#include "Core/Util/HashUtils.h"

#include <d3dx12.h>
#include <dxcapi.h>
#include <d3dcompiler.h> 
#include <d3d12shader.h>

using Microsoft::WRL::ComPtr;


namespace
{
	constexpr D3D12_SHADER_VISIBILITY GetShaderVisibilityFlagFromShaderType(re::Shader::ShaderType shaderType)
	{
		switch (shaderType)
		{
		case re::Shader::ShaderType::Vertex:		return D3D12_SHADER_VISIBILITY_VERTEX;
		case re::Shader::ShaderType::Geometry:		return D3D12_SHADER_VISIBILITY_GEOMETRY;
		case re::Shader::ShaderType::Pixel:			return D3D12_SHADER_VISIBILITY_PIXEL;
		case re::Shader::ShaderType::Hull:			return D3D12_SHADER_VISIBILITY_HULL;
		case re::Shader::ShaderType::Domain:		return D3D12_SHADER_VISIBILITY_DOMAIN;
		case re::Shader::ShaderType::Amplification:	return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
		case re::Shader::ShaderType::Mesh:			return D3D12_SHADER_VISIBILITY_MESH;
		
		case re::Shader::ShaderType::Compute: // Fall back to D3D12_SHADER_VISIBILITY_ALL
		case re::Shader::ShaderType::HitGroup_Intersection:
		case re::Shader::ShaderType::HitGroup_AnyHit:
		case re::Shader::ShaderType::HitGroup_ClosestHit:
		case re::Shader::ShaderType::Callable:
		case re::Shader::ShaderType::RayGen:
		case re::Shader::ShaderType::Miss:
			return D3D12_SHADER_VISIBILITY_ALL;

		default: return D3D12_SHADER_VISIBILITY_ALL; // This should never happen
		}
		SEStaticAssert(re::Shader::ShaderType_Count == 14, "Must update this function if ShaderType enum has changed");
	}


	constexpr D3D12_DESCRIPTOR_RANGE_TYPE GetD3DRangeType(dx12::RootSignature::DescriptorType descType)
	{
		switch (descType)
		{
		case dx12::RootSignature::DescriptorType::SRV: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case dx12::RootSignature::DescriptorType::UAV: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		case dx12::RootSignature::DescriptorType::CBV: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		default: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV; // This should never happen
		}
		SEStaticAssert(dx12::RootSignature::DescriptorType::Type_Count == 3,
			"Must update this function if DescriptorType enum has changed");
	}


	constexpr D3D12_SRV_DIMENSION GetD3D12SRVDimension(D3D_SRV_DIMENSION srvDimension)
	{
		// D3D_SRV_DIMENSION::D3D_SRV_DIMENSION_BUFFEREX (== 11, raw buffer resource) is handled differently in D3D12
		SEAssert(srvDimension >= D3D_SRV_DIMENSION_UNKNOWN && srvDimension <= D3D_SRV_DIMENSION_TEXTURECUBEARRAY,
			"D3D_SRV_DIMENSION does not have a (known) D3D12_SRV_DIMENSION equivalent");
		return static_cast<D3D12_SRV_DIMENSION>(srvDimension);
	}


	constexpr D3D12_UAV_DIMENSION GetD3D12UAVDimension(D3D_SRV_DIMENSION uavDimension)
	{
		SEAssert(uavDimension >= D3D_SRV_DIMENSION_UNKNOWN && uavDimension <= D3D_SRV_DIMENSION_TEXTURE3D,
			"D3D_SRV_DIMENSION does not have a (known) D3D12_UAV_DIMENSION equivalent");
		return static_cast<D3D12_UAV_DIMENSION>(uavDimension);
	}


	constexpr DXGI_FORMAT GetFormatFromReturnType(D3D_RESOURCE_RETURN_TYPE returnType)
	{
		switch (returnType)
		{
			case D3D_RETURN_TYPE_UNORM:		return DXGI_FORMAT_R8G8B8A8_UNORM;
			case D3D_RETURN_TYPE_SNORM:		return DXGI_FORMAT_R8G8B8A8_SNORM;
			case D3D_RETURN_TYPE_SINT:		return DXGI_FORMAT_R8G8B8A8_SINT;
			case D3D_RETURN_TYPE_UINT:		return DXGI_FORMAT_R8G8B8A8_UINT;
			case D3D_RETURN_TYPE_FLOAT:		return DXGI_FORMAT_R32G32B32A32_FLOAT;
			case D3D_RETURN_TYPE_MIXED:		return DXGI_FORMAT_R32G32B32A32_UINT; // Best guess
			case D3D_RETURN_TYPE_DOUBLE:
			case D3D_RETURN_TYPE_CONTINUED:
			default: SEAssertF("Unexpected return type");
		}
		return DXGI_FORMAT_R8G8B8A8_UNORM; // This should never happen
	}


	uint64_t HashRootSigDesc(D3D12_VERSIONED_ROOT_SIGNATURE_DESC const& rootSigDesc)
	{
		uint64_t hash = 0;

		switch (rootSigDesc.Version)
		{
		case D3D_ROOT_SIGNATURE_VERSION_1_0:
		{
			SEAssertF("TODO: Support this");
		}
		break;
		case D3D_ROOT_SIGNATURE_VERSION_1_1:
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
						util::AddDataToHash(hash, descriptorTable.pDescriptorRanges[rangeIdx].Flags);
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
				default: SEAssertF("Invalid parameter type");
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
		case D3D_ROOT_SIGNATURE_VERSION_1_2:
		{
			SEAssertF("TODO: Support this");
		}
		break;
		default:
			SEAssertF("Invalid root signature version");
		}

		return hash;
	}


	D3D12_ROOT_SIGNATURE_FLAGS BuildRootSignatureFlags(
		std::array<Microsoft::WRL::ComPtr<ID3DBlob>, re::Shader::ShaderType_Count> const& shaderBlobs)
	{
		// Start by adding all the deny flags: We'll selectively remove them if we encounter a conflicting shader type
		D3D12_ROOT_SIGNATURE_FLAGS flags = 
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS|
			D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

		// Allow direct indexing by default:
		flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
		flags |= D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

		for (uint8_t shaderIdx = 0; shaderIdx < re::Shader::ShaderType_Count; shaderIdx++)
		{
			if (shaderBlobs[shaderIdx] == nullptr)
			{
				continue;
			}

			switch (shaderIdx)
			{
				case re::Shader::Vertex:
				{
					flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
					flags ^= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
				}
				break;
				case re::Shader::Geometry:
				{
					flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;
					flags ^= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
				}
				break;
				case re::Shader::Pixel:
				{
					flags ^= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
				}
				break;
				case re::Shader::Hull:
				{
					flags ^= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
				}
				break;
				case re::Shader::Domain:
				{
					flags ^= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
				}
				break;
				case re::Shader::Amplification:
				{
					flags ^= D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS;
				}
				break;
				case re::Shader::Mesh:
				{
					flags ^= D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
				}
				break;
				case re::Shader::Compute:
				{
					// Nothing to change
				}
				break;
				case re::Shader::HitGroup_Intersection:
				case re::Shader::HitGroup_AnyHit:
				case re::Shader::HitGroup_ClosestHit:
				case re::Shader::Callable:
				case re::Shader::RayGen:
				case re::Shader::Miss:
				{
					flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE; // Can't be combined with other flags
				}
				break;
				default: SEAssertF("Invalid shader type");
			}
		}
		return flags;
	}


	void ValidateDescriptorRangeSizes(std::vector<dx12::RootSignature::DescriptorTable> const& descriptorTableMetadata)
	{
		SEStaticAssert(dx12::RootSignature::Type_Count == 3,
			"Root signature descriptor type count has changed. This function must be updated");

#if defined(_DEBUG)
		std::array<uint32_t, dx12::RootSignature::Type_Count> descriptorTypeCounts{}; // Value initializes as 0

		for (auto const& entry : descriptorTableMetadata)
		{
			for (uint8_t descriptorTypeIdx = 0; descriptorTypeIdx < dx12::RootSignature::Type_Count; ++descriptorTypeIdx)
			{
				for (auto const& range : entry.m_ranges[descriptorTypeIdx])
				{
					descriptorTypeCounts[descriptorTypeIdx] += range.m_bindCount;
				}
			}
		}

		SEAssert(descriptorTypeCounts[dx12::RootSignature::SRV] < dx12::SysInfo::GetMaxDescriptorTableSRVs(),
			"More SRVs requested than allowed across all descriptor tables per shader stage");

		SEAssert(descriptorTypeCounts[dx12::RootSignature::UAV] < dx12::SysInfo::GetMaxDescriptorTableUAVs(),
			"More UAVs requested than allowed across all descriptor tables per shader stage");

		SEAssert(descriptorTypeCounts[dx12::RootSignature::CBV] < dx12::SysInfo::GetMaxDescriptorTableCBVs(),
			"More CBVs requested than allowed across all descriptor tables per shader stage");

		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-support
#endif
	}
}

namespace dx12
{
	RootSignature::RootSignature()
		: m_rootSignature(nullptr)
		, m_rootSigDescHash(0)
		, m_rootSigDescriptorTableIdxBitmask(0)
	{
		// Zero our descriptor table entry counters: For each root sig. index containing a descriptor table, this tracks
		// how many descriptors are in that table
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
		SEAssert(rootParam.m_index != k_invalidRootSigIndex &&
			rootParam.m_type != RootParameter::Type::Type_Invalid &&
			rootParam.m_registerBindPoint != k_invalidRegisterVal &&
			rootParam.m_registerSpace != k_invalidRegisterVal,
			"RootParameter is not fully initialized");

		SEAssert(rootParam.m_type != RootParameter::Type::Constant || 
				(rootParam.m_rootConstant.m_num32BitValues != k_invalidCount &&
				rootParam.m_rootConstant.m_destOffsetIn32BitValues != k_invalidOffset),
			"Constant union is not fully initialized");

		SEAssert(rootParam.m_type != RootParameter::Type::DescriptorTable || 
				(rootParam.m_tableEntry.m_type != DescriptorType::Type_Invalid &&
					rootParam.m_tableEntry.m_offset != k_invalidOffset && 
					(rootParam.m_tableEntry.m_type == DescriptorType::CBV ||
						rootParam.m_tableEntry.m_srvViewDimension != 0)), // It's a union, either member should be > 0
			"TableEntry is not fully initialized"); 

		const size_t metadataIdx = m_rootParams.size();

		// Map the name to the insertion index:
		auto const& insertResult = m_namesToRootParamsIdx.emplace(name, metadataIdx);
		SEAssert(insertResult.second == true, "Name mapping metadata already exists");

		// Finally, move the root param into our vector
		m_rootParams.emplace_back(std::move(rootParam));
	}


	void RootSignature::ParseInputBindingDesc(
		dx12::RootSignature* newRootSig,
		re::Shader::ShaderType shaderType,
		D3D12_SHADER_INPUT_BIND_DESC const& inputBindingDesc,
		std::array<std::vector<RangeInput>, DescriptorType::Type_Count>& rangeInputs,
		std::vector<CD3DX12_ROOT_PARAMETER1>& rootParameters,
		std::vector<D3D12_STATIC_SAMPLER_DESC>& staticSamplers)
	{
		auto AddRangeInput = [&rangeInputs, &inputBindingDesc, &shaderType]
			(dx12::RootSignature::DescriptorType descriptorType)
			{
				// Check to see if our resource has already been added (e.g. if it's referenced in multiple shader
				// stages). We do a linear search, but in practice the no. of elements is likely very small
				auto result = std::find_if( // Find matching names:
					rangeInputs[descriptorType].begin(),
					rangeInputs[descriptorType].end(),
					[&inputBindingDesc](auto const& a) { return strcmp(a.m_name.c_str(), inputBindingDesc.Name) == 0; });

				if (result == rangeInputs[descriptorType].end())
				{
					RangeInput& newRangeInput = rangeInputs[descriptorType].emplace_back(inputBindingDesc);
					newRangeInput.m_name = inputBindingDesc.Name; // Copy the name before it goes out of scope
					newRangeInput.m_visibility = GetShaderVisibilityFlagFromShaderType(shaderType);
				}
				else
				{
					SEAssert(result->BindPoint == inputBindingDesc.BindPoint &&
						result->Space == inputBindingDesc.Space &&
						result->Type == inputBindingDesc.Type &&
						result->BindCount == inputBindingDesc.BindCount &&
						result->ReturnType == inputBindingDesc.ReturnType &&
						result->Dimension == inputBindingDesc.Dimension &&
						result->NumSamples == inputBindingDesc.NumSamples,
						"Found resource with the same name but a different binding description");

					result->m_visibility = D3D12_SHADER_VISIBILITY_ALL;
				}
			};


		switch (inputBindingDesc.Type)
		{
		case D3D_SIT_RTACCELERATIONSTRUCTURE:
		{
			if (inputBindingDesc.BindCount > 1)
			{
				AddRangeInput(dx12::RootSignature::DescriptorType::SRV);
			}
			else // Single RT AS: Bind in the root signature
			{
				if (!newRootSig->m_namesToRootParamsIdx.contains(inputBindingDesc.Name))
				{
					const uint8_t rootIdx = util::CheckedCast<uint8_t>(rootParameters.size());
					CD3DX12_ROOT_PARAMETER1& newRootParam = rootParameters.emplace_back();

					newRootParam.InitAsShaderResourceView(
						inputBindingDesc.BindPoint,			// Shader register
						inputBindingDesc.Space,				// Register space
						D3D12_ROOT_DESCRIPTOR_FLAG_NONE,	// Flags
						D3D12_SHADER_VISIBILITY_ALL);		// Shader visibility: AS's are always visible

					newRootSig->InsertNewRootParamMetadata(
						inputBindingDesc.Name,
						RootParameter{
							.m_index = rootIdx,
							.m_type = RootParameter::Type::SRV,
							.m_registerBindPoint = util::CheckedCast<uint8_t>(inputBindingDesc.BindPoint),
							.m_registerSpace = util::CheckedCast<uint8_t>(inputBindingDesc.Space),
							.m_rootSRV{ 
								.m_viewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE,
							}
						});
				}
				else
				{
					const size_t metadataIdx = newRootSig->m_namesToRootParamsIdx[inputBindingDesc.Name];
					rootParameters[newRootSig->m_rootParams[metadataIdx].m_index].ShaderVisibility =
						D3D12_SHADER_VISIBILITY_ALL;
				}
			}
		}
		break;
		case D3D_SIT_UAV_FEEDBACKTEXTURE:
		{
			SEAssertF("TODO: Handle this resource type");
		}
		break;
		case D3D_SIT_CBUFFER: // The shader resource is a constant buffer
		{
			SEAssert(strcmp(inputBindingDesc.Name, "$Globals") != 0, "TODO: Handle root constants");

			if (inputBindingDesc.BindCount > 1)
			{
				AddRangeInput(dx12::RootSignature::DescriptorType::CBV);
			}
			else
			{
				if (!newRootSig->m_namesToRootParamsIdx.contains(inputBindingDesc.Name))
				{
					const uint8_t rootIdx = util::CheckedCast<uint8_t>(rootParameters.size());
					rootParameters.emplace_back();

					rootParameters[rootIdx].InitAsConstantBufferView(
						inputBindingDesc.BindPoint,	// Shader register
						inputBindingDesc.Space,		// Register space
						D3D12_ROOT_DESCRIPTOR_FLAGS::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,	// Flags. TODO: Is volatile always appropriate?
						GetShaderVisibilityFlagFromShaderType(shaderType));	// Shader visibility

					newRootSig->InsertNewRootParamMetadata(inputBindingDesc.Name,
						RootParameter{
							.m_index = rootIdx,
							.m_type = RootParameter::Type::CBV,
							.m_registerBindPoint = util::CheckedCast<uint8_t>(inputBindingDesc.BindPoint),
							.m_registerSpace = util::CheckedCast<uint8_t>(inputBindingDesc.Space) });
				}
				else
				{
					const size_t metadataIdx = newRootSig->m_namesToRootParamsIdx[inputBindingDesc.Name];
					rootParameters[newRootSig->m_rootParams[metadataIdx].m_index].ShaderVisibility =
						D3D12_SHADER_VISIBILITY_ALL;
				}
			}
		}
		break;
		case D3D_SIT_TBUFFER: // The shader resource is a texture buffer
		{
			SEAssertF("TODO: Handle this resource type");
		}
		break;
		case D3D_SIT_TEXTURE: // The shader resource is a texture
		{
			AddRangeInput(dx12::RootSignature::DescriptorType::SRV);
		}
		break;
		case D3D_SIT_SAMPLER: // The shader resource is a sampler
		{
			core::InvPtr<re::Sampler> const& sampler = re::Sampler::GetSampler(inputBindingDesc.Name);

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
					GetShaderVisibilityFlagFromShaderType(shaderType);

				SEAssert(staticSamplers.size() <= 2032,
					"The maximum number of unique static samplers across live root signatures is 2032 (+16 reserved "
					"for drivers that need their own samplers)");
			}
			else
			{
				result->ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}
		}
		break;
		case D3D_SIT_UAV_RWTYPED: // RW buffer/texture (e.g.RWTexture2D)
		{
			// We can only bind this as a range as UAV root descriptors can only be Raw or Structured buffers
			AddRangeInput(dx12::RootSignature::DescriptorType::UAV);
		}
		break;
		case D3D_SIT_UAV_RWSTRUCTURED: // RW structured buffer
		{
			if (inputBindingDesc.BindCount > 1) // RWStructured buffer arrays: Bind as a range
			{
				AddRangeInput(dx12::RootSignature::DescriptorType::UAV);
			}
			else // Single RWStructured buffer: Bind in the root signature
			{
				if (!newRootSig->m_namesToRootParamsIdx.contains(inputBindingDesc.Name))
				{
					const uint8_t rootIdx = util::CheckedCast<uint8_t>(rootParameters.size());
					rootParameters.emplace_back();

					rootParameters[rootIdx].InitAsUnorderedAccessView(
						inputBindingDesc.BindPoint,	// Shader register
						inputBindingDesc.Space,		// Register space
						D3D12_ROOT_DESCRIPTOR_FLAGS::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,	// Flags. TODO: Is volatile always appropriate?
						GetShaderVisibilityFlagFromShaderType(shaderType));	// Shader visibility

					newRootSig->InsertNewRootParamMetadata(inputBindingDesc.Name,
						RootParameter{
							.m_index = rootIdx,
							.m_type = RootParameter::Type::UAV,
							.m_registerBindPoint = util::CheckedCast<uint8_t>(inputBindingDesc.BindPoint),
							.m_registerSpace = util::CheckedCast<uint8_t>(inputBindingDesc.Space),
							.m_rootUAV{
								.m_viewDimension = GetD3D12UAVDimension(inputBindingDesc.Dimension)
							}
						});
				}
				else
				{
					const size_t metadataIdx = newRootSig->m_namesToRootParamsIdx[inputBindingDesc.Name];
					rootParameters[newRootSig->m_rootParams[metadataIdx].m_index].ShaderVisibility =
						D3D12_SHADER_VISIBILITY_ALL;
				}
			}
		}
		break;
		case D3D_SIT_STRUCTURED: // Structured buffer
		{
			if (inputBindingDesc.BindCount > 1) // Structured buffer arrays: Bind as a range
			{
				AddRangeInput(dx12::RootSignature::DescriptorType::SRV);
			}
			else // Single structured buffer: Bind in the root signature
			{
				if (!newRootSig->m_namesToRootParamsIdx.contains(inputBindingDesc.Name))
				{
					const uint8_t rootIdx = util::CheckedCast<uint8_t>(rootParameters.size());
					rootParameters.emplace_back();

					rootParameters[rootIdx].InitAsShaderResourceView(
						inputBindingDesc.BindPoint,	// Shader register
						inputBindingDesc.Space,		// Register space
						D3D12_ROOT_DESCRIPTOR_FLAGS::D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,	// Flags. TODO: Is volatile always appropriate?
						GetShaderVisibilityFlagFromShaderType(shaderType));	// Shader visibility

					newRootSig->InsertNewRootParamMetadata(inputBindingDesc.Name,
						RootParameter{
							.m_index = rootIdx,
							.m_type = RootParameter::Type::SRV,
							.m_registerBindPoint = util::CheckedCast<uint8_t>(inputBindingDesc.BindPoint),
							.m_registerSpace = util::CheckedCast<uint8_t>(inputBindingDesc.Space),
							.m_rootSRV{
								.m_viewDimension = GetD3D12SRVDimension(inputBindingDesc.Dimension)
							}
						});
				}
				else
				{
					const size_t metadataIdx = newRootSig->m_namesToRootParamsIdx[inputBindingDesc.Name];
					rootParameters[newRootSig->m_rootParams[metadataIdx].m_index].ShaderVisibility =
						D3D12_SHADER_VISIBILITY_ALL;
				}
			}
		}
		break;
		case D3D_SIT_BYTEADDRESS: // Byte-address buffer
		case D3D_SIT_UAV_RWBYTEADDRESS: // RW byte-address buffer
		case D3D_SIT_UAV_APPEND_STRUCTURED: // Append-structured buffer
		case D3D_SIT_UAV_CONSUME_STRUCTURED: // Consume-structured buffer
		case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: // RW structured buffer with built-in counter to append or consume
		{
			SEAssertF("TODO: Handle this resource type");
		}
		break;
		default: SEAssertF("Invalid resource type");
		}
	}


	std::unique_ptr<dx12::RootSignature> RootSignature::Create(re::Shader const& shader)
	{
		dx12::Shader::PlatformParams* shaderPlatParams = shader.GetPlatformParams()->As<dx12::Shader::PlatformParams*>();
		SEAssert(shaderPlatParams->m_isCreated, "Shader must be created");

		std::unique_ptr<dx12::RootSignature> newRootSig;
		newRootSig.reset(new dx12::RootSignature());

		// We record details of descriptors we want to place into descriptor tables, and then build the tables later
		std::array<std::vector<RangeInput>, DescriptorType::Type_Count> rangeInputs;

		constexpr size_t k_expectedNumberOfSamplers = 16; // Resource tier 1
		std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;
		staticSamplers.reserve(k_expectedNumberOfSamplers);

		std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters;
		rootParameters.reserve(k_totalRootSigDescriptorTableIndices);

		// DxcUtils for shader/library reflection:
		ComPtr<IDxcUtils> dxcUtils;
		CheckHResult(::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils)), "Failed to create IDxcUtils instance");

		if (shader.GetPipelineType() == re::Shader::PipelineType::RayTracing) // Library reflection
		{
			ComPtr<ID3D12LibraryReflection> libReflection;
			for (uint8_t shaderIdx = 0; shaderIdx < re::Shader::ShaderType_Count; shaderIdx++)
			{
				if (shaderPlatParams->m_shaderBlobs[shaderIdx] == nullptr)
				{
					continue;
				}
				
				const DxcBuffer reflectionBuffer
				{
					.Ptr = shaderPlatParams->m_shaderBlobs[shaderIdx]->GetBufferPointer(),
					.Size = shaderPlatParams->m_shaderBlobs[shaderIdx]->GetBufferSize(),
					.Encoding = 0, // 0 = non-text bytes
				};
				CheckHResult(dxcUtils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&libReflection)),
					"Failed to reflect D3D12 library");

				// Get the Library description:
				D3D12_LIBRARY_DESC libraryDesc{};
				CheckHResult(libReflection->GetDesc(&libraryDesc), "Failed to get library description");

				// Parse each function:
				for (uint32_t funcIdx = 0; funcIdx < libraryDesc.FunctionCount; ++funcIdx)
				{
					ID3D12FunctionReflection* funcReflection = libReflection->GetFunctionByIndex(funcIdx);

					D3D12_FUNCTION_DESC functionDesc{};
					CheckHResult(funcReflection->GetDesc(&functionDesc), "Failed to get function description");

					// Bound resources:
					D3D12_SHADER_INPUT_BIND_DESC inputBindingDesc{};
					for (uint32_t resourceIdx = 0; resourceIdx < functionDesc.BoundResources; ++resourceIdx)
					{
						CheckHResult(funcReflection->GetResourceBindingDesc(resourceIdx, &inputBindingDesc),
							"Failed to get resource binding description");

						ParseInputBindingDesc(
							newRootSig.get(),
							static_cast<re::Shader::ShaderType>(shaderIdx),
							inputBindingDesc,
							rangeInputs,
							rootParameters,
							staticSamplers);
					}
				}
			}
		}
		else // Shader reflection:
		{
			ComPtr<ID3D12ShaderReflection> shaderReflection;
			for (uint8_t shaderIdx = 0; shaderIdx < re::Shader::ShaderType_Count; shaderIdx++)
			{
				if (shaderPlatParams->m_shaderBlobs[shaderIdx] == nullptr)
				{
					continue;
				}

				// Get the reflection for the current shader stage:
				const DxcBuffer reflectionBuffer
				{
					.Ptr = shaderPlatParams->m_shaderBlobs[shaderIdx]->GetBufferPointer(),
					.Size = shaderPlatParams->m_shaderBlobs[shaderIdx]->GetBufferSize(),
					.Encoding = 0, // 0 = non-text bytes
				};

				CheckHResult(dxcUtils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&shaderReflection)),
					"Failed to reflect shader");

				D3D12_SHADER_DESC shaderDesc{};
				dx12::CheckHResult(shaderReflection->GetDesc(&shaderDesc), "Failed to get shader description");

				// Parse the resource bindings for the current shader stage:
				D3D12_SHADER_INPUT_BIND_DESC inputBindingDesc{};
				for (uint32_t currentResource = 0; currentResource < shaderDesc.BoundResources; currentResource++)
				{
					CheckHResult(shaderReflection->GetResourceBindingDesc(currentResource, &inputBindingDesc),
						"Failed to get resource binding description");

					SEAssert(rootParameters.size() < std::numeric_limits<uint8_t>::max(),
						"Too many root parameters. Consider increasing the root sig index type from a uint8_t");

					ParseInputBindingDesc(
						newRootSig.get(),
						static_cast<re::Shader::ShaderType>(shaderIdx),
						inputBindingDesc,
						rangeInputs,
						rootParameters,
						staticSamplers);
				}
			}
		}


		// TODO: Sort rootParameters based on the .ParameterType, to ensure optimal/preferred ordering/grouping of entries
		// -> MS recommends binding the most frequently changing elements at the start of the root signature.
		//		-> For SaberEngine, that's probably buffers: CBVs and SRVs


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
				rangeInputs[rangeType].begin(),
				rangeInputs[rangeType].end(),
				[](auto const& a, auto const& b)
				{
					if (a.BindPoint == b.BindPoint)
					{
						SEAssert(a.Space != b.Space, "Register collision");
						return a.Space < b.Space;
					}
					return a.BindPoint < b.BindPoint;
				});

			// We're going to build a descriptor table entry at the current root index:
			const uint8_t rootIdx = util::CheckedCast<uint8_t>(rootParameters.size());
			rootParameters.emplace_back();

	
			uint32_t totalRangeDescriptors = 0; // How many descriptors in the entire range

			// Walk through the sorted descriptors, and build ranges from contiguous blocks:
			size_t rangeStart = 0;
			size_t rangeEnd = 1;
			std::vector<std::string> namesInRange;
			D3D12_SHADER_VISIBILITY tableVisibility = rangeInputs[rangeType][rangeStart].m_visibility;
			while (rangeStart < rangeInputs[rangeType].size())
			{
				// Store the names in order so we can update the binding metadata later:
				namesInRange.emplace_back(rangeInputs[rangeType][rangeStart].m_name);

				uint8_t numDescriptors = rangeInputs[rangeType][rangeStart].BindCount;
				uint8_t expectedNextRegister = rangeInputs[rangeType][rangeStart].BindPoint + numDescriptors;

				// Find the end of the current contiguous range:
				while (rangeEnd < rangeInputs[rangeType].size() &&
					rangeInputs[rangeType][rangeEnd].BindPoint == expectedNextRegister &&
					rangeInputs[rangeType][rangeEnd].Space == rangeInputs[rangeType][rangeStart].Space)
				{
					namesInRange.emplace_back(rangeInputs[rangeType][rangeEnd].m_name);

					if (rangeInputs[rangeType][rangeEnd].m_visibility != tableVisibility)
					{
						tableVisibility = D3D12_SHADER_VISIBILITY_ALL;
					}

					numDescriptors += rangeInputs[rangeType][rangeEnd].BindCount;
					expectedNextRegister += rangeInputs[rangeType][rangeEnd].BindCount;

					rangeEnd++;
				}

				totalRangeDescriptors += numDescriptors;

				// Initialize the descriptor range:
				const D3D12_DESCRIPTOR_RANGE_TYPE d3dRangeType = GetD3DRangeType(rangeType);
				
				CD3DX12_DESCRIPTOR_RANGE1& newD3DDescriptorRange = tableRanges[rangeType].emplace_back();

				const uint32_t baseRegister = rangeInputs[rangeType][rangeStart].BindPoint;
				const uint32_t registerSpace = rangeInputs[rangeType][rangeStart].Space;		

				newD3DDescriptorRange.Init(
					d3dRangeType,
					numDescriptors,
					baseRegister,
					registerSpace,
					D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); //  TODO: Is this flag appropriate?

				// Populate the descriptor metadata:
				DescriptorTable* newDescriptorTable = nullptr;
				uint8_t baseRegisterOffset = 0; // We are processing contiguous ranges of registers only
				for (size_t rangeIdx = rangeStart; rangeIdx < rangeEnd; rangeIdx++)
				{
					// Create the binding metadata for our individual RootParameter descriptor table entries:
					RootParameter rootParameter{
						.m_index = rootIdx,
						.m_type = RootParameter::Type::DescriptorTable,
						.m_registerBindPoint = util::CheckedCast<uint8_t>(baseRegister + baseRegisterOffset++),
						.m_registerSpace = util::CheckedCast<uint8_t>(registerSpace),
						.m_tableEntry = RootSignature::TableEntry{
							.m_type = rangeType,
							.m_offset = util::CheckedCast<uint8_t>(rangeIdx),
						}
					};

					// Create a single metadata entry for the contiguous range of descriptors within DescriptorTables:
					bool isNewRange = false;
					if (rangeIdx == rangeStart ||
						rangeInputs[rangeType][rangeIdx].ReturnType != rangeInputs[rangeType][rangeStart].ReturnType ||
						rangeInputs[rangeType][rangeIdx].Dimension != rangeInputs[rangeType][rangeStart].Dimension)
					{
						newDescriptorTable = &newRootSig->m_descriptorTables.emplace_back();
						newDescriptorTable->m_index = rootIdx;

						isNewRange = true;
					}

					// Populate the descriptor table metadata:
					switch (rangeType)
					{
					case DescriptorType::SRV:
					{
						const D3D12_SRV_DIMENSION d3d12SrvDimension =
							GetD3D12SRVDimension(rangeInputs[rangeType][rangeIdx].Dimension);

						rootParameter.m_tableEntry.m_srvViewDimension = d3d12SrvDimension;

						if (isNewRange)
						{
							newDescriptorTable->m_ranges[DescriptorType::SRV].emplace_back(RangeEntry{
								.m_bindCount = numDescriptors,
								.m_srvDesc = {
									.m_format = GetFormatFromReturnType(rangeInputs[rangeType][rangeIdx].ReturnType),
									.m_viewDimension = d3d12SrvDimension,}
								});
						}
					}
					break;
					case DescriptorType::UAV:
					{
						const D3D12_UAV_DIMENSION d3d12UavDimension =
							GetD3D12UAVDimension(rangeInputs[rangeType][rangeIdx].Dimension);

						rootParameter.m_tableEntry.m_uavViewDimension = d3d12UavDimension;

						if (isNewRange)
						{
							newDescriptorTable->m_ranges[DescriptorType::UAV].emplace_back(RangeEntry{
								.m_bindCount = numDescriptors,
								.m_uavDesc = {
									.m_format = GetFormatFromReturnType(rangeInputs[rangeType][rangeIdx].ReturnType),
									.m_viewDimension = d3d12UavDimension,}
								});
						}
					}
					break;
					case DescriptorType::CBV:
					{
						if (isNewRange)
						{
							newDescriptorTable->m_ranges[DescriptorType::CBV].emplace_back(RangeEntry{
								.m_bindCount = numDescriptors,
							});
						}
					}
					break;
					default:
						SEAssertF("Invalid range type");
					}

					newRootSig->InsertNewRootParamMetadata(namesInRange[rangeIdx].c_str(), std::move(rootParameter));
				} // end rangeIdx loop

				// Prepare for the next iteration:
				rangeStart = rangeEnd;
				rangeEnd++;
			}

			ValidateDescriptorRangeSizes(newRootSig->m_descriptorTables); // _DEBUG only

			// How many individual descriptor tables we're creating for the current range type:
			const uint32_t numDescriptorRanges = util::CheckedCast<uint32_t>(tableRanges[rangeType].size());

			// Initialize the root parameter as a descriptor table built from our ranges:
			rootParameters[rootIdx].InitAsDescriptorTable(
				numDescriptorRanges,
				tableRanges[rangeType].data(),
				tableVisibility);
		
			// How many descriptors are in the table stored at the given root sig index (i.e. including > 1 bind counts):
			newRootSig->m_numDescriptorsPerTable[rootIdx] = totalRangeDescriptors;
			
			const uint32_t descriptorTableBitmask = (1 << rootIdx);
			newRootSig->m_rootSigDescriptorTableIdxBitmask |= descriptorTableBitmask;

		} // End descriptor table DescriptorType loop


		// Allow/deny unnecessary shader access
		const D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = BuildRootSignatureFlags(shaderPlatParams->m_shaderBlobs);

		// TODO: Support multiple root signature versions. For now, we just choose v1.1
		const D3D_ROOT_SIGNATURE_VERSION rootSigVersion = SysInfo::GetHighestSupportedRootSignatureVersion();
		SEAssert(static_cast<uint32_t>(rootSigVersion) >= 0x2,
			"System does not support D3D_ROOT_SIGNATURE_VERSION_1_1 or above");

		// Create the root signature description from our array of root parameters:
		D3D12_ROOT_PARAMETER1 const* rootParamsPtr = rootParameters.empty() ? nullptr : rootParameters.data();
		D3D12_STATIC_SAMPLER_DESC const* staticSamplersPtr = staticSamplers.empty() ? nullptr : staticSamplers.data();

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription{};
		rootSignatureDescription.Init_1_1(
			util::CheckedCast<uint32_t>(rootParameters.size()),	// Num parameters
			rootParamsPtr,										// const D3D12_ROOT_PARAMETER1*
			util::CheckedCast<uint32_t>(staticSamplers.size()),	// Num static samplers
			staticSamplersPtr,									// const D3D12_STATIC_SAMPLER_DESC*
			rootSignatureFlags);								// D3D12_ROOT_SIGNATURE_FLAGS

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		// Before we create a root signature, check if one with the same layout already exists:
		newRootSig->m_rootSigDescHash = HashRootSigDesc(rootSignatureDescription);
		if (context->HasRootSignature(newRootSig->m_rootSigDescHash))
		{
			newRootSig->m_rootSignature = context->GetRootSignature(newRootSig->m_rootSigDescHash);
		}
		else
		{
			// Serialize the root signature:
			ComPtr<ID3DBlob> rootSignatureBlob = nullptr;
			ComPtr<ID3DBlob> errorBlob = nullptr;
			HRESULT hr = D3DX12SerializeVersionedRootSignature(
				&rootSignatureDescription,
				rootSigVersion,
				&rootSignatureBlob,
				&errorBlob);			
			CheckHResult(hr, errorBlob ? 
				static_cast<const char*>(errorBlob->GetBufferPointer()) : 
				"Failed to serialize versioned root signature");

			// Create the root signature:
			ID3D12Device* device = context->GetDevice().GetD3DDevice().Get();

			hr = device->CreateRootSignature(
				dx12::SysInfo::GetDeviceNodeMask(),
				rootSignatureBlob->GetBufferPointer(),
				rootSignatureBlob->GetBufferSize(),
				IID_PPV_ARGS(&newRootSig->m_rootSignature));
			CheckHResult(hr, "Failed to create root signature");

			const std::wstring rootSigName = shader.GetWName() + L"_RootSig";
			newRootSig->m_rootSignature->SetName(rootSigName.c_str());

			// Add the new root sig to the library:
			context->AddRootSignature(newRootSig->m_rootSigDescHash, newRootSig->m_rootSignature);
		}

		return newRootSig;
	}


	RootSignature::RootParameter const* RootSignature::GetRootSignatureEntry(std::string const& resourceName) const
	{
		auto const& result = m_namesToRootParamsIdx.find(resourceName);
		const bool hasResource = result != m_namesToRootParamsIdx.end();

		SEAssert(hasResource || 
			core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
			"Root signature does not contain a parameter with that name");

		return hasResource ? &m_rootParams[result->second] : nullptr;
	}


#if defined(_DEBUG)
	bool RootSignature::HasResource(std::string const& resourceName) const
	{
		return m_namesToRootParamsIdx.contains(resourceName);
	}
#endif


#if defined(_DEBUG)
	std::string const& RootSignature::DebugGetNameFromRootParamIdx(uint8_t rootParamsIdx) const
	{
		for (auto const& entry : m_namesToRootParamsIdx)
		{
			if (entry.second == rootParamsIdx)
			{
				return entry.first;
			}
		}
		static std::string s_errorMsg = "Invalid root param index, no name found";
		return s_errorMsg;
	}
#endif
}