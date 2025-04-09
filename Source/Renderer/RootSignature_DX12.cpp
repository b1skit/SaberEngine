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

using Microsoft::WRL::ComPtr;


namespace
{
	constexpr size_t k_expectedNumberOfSamplers = 16; // Resource tier 1


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
		case dx12::RootSignature::DescriptorType::CBV: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		case dx12::RootSignature::DescriptorType::SRV: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case dx12::RootSignature::DescriptorType::UAV: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		default: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV; // This should never happen
		}
		SEStaticAssert(dx12::RootSignature::DescriptorType::Type_Count == 3,
			"Must update this function if DescriptorType enum has changed");
	}


	constexpr D3D12_SRV_DIMENSION GetD3D12SRVDimension(D3D_SRV_DIMENSION srvDimension)
	{
		SEAssert(srvDimension >= D3D_SRV_DIMENSION_UNKNOWN && srvDimension <= D3D_SRV_DIMENSION_BUFFEREX,
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
				case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
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
				case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
				{
					D3D12_ROOT_CONSTANTS const& rootConstant = rootSigDesc.Desc_1_1.pParameters[paramIdx].Constants;
					util::AddDataToHash(hash, rootConstant.ShaderRegister);
					util::AddDataToHash(hash, rootConstant.RegisterSpace);
					util::AddDataToHash(hash, rootConstant.Num32BitValues);
				}
				break;
				case D3D12_ROOT_PARAMETER_TYPE_CBV:
				case D3D12_ROOT_PARAMETER_TYPE_SRV:
				case D3D12_ROOT_PARAMETER_TYPE_UAV:
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


	static constexpr dx12::RootSignature::DescriptorType D3DDescriptorRangeTypeToDescriptorType(
		D3D12_DESCRIPTOR_RANGE_TYPE type)
	{
		SEStaticAssert(dx12::RootSignature::Type_Count == 3,
			"Root signature descriptor type count has changed. This function must be updated");

		switch (type)
		{
		case D3D12_DESCRIPTOR_RANGE_TYPE_SRV: return dx12::RootSignature::DescriptorType::SRV;
		case D3D12_DESCRIPTOR_RANGE_TYPE_UAV: return dx12::RootSignature::DescriptorType::UAV;
		case D3D12_DESCRIPTOR_RANGE_TYPE_CBV: return dx12::RootSignature::DescriptorType::CBV;
		default: return dx12::RootSignature::DescriptorType::Type_Invalid; // This should never happen
		}		
	}


	bool IsUnboundedRange(dx12::RootSignature::DescriptorType rangeType, uint32_t bindPoint, uint32_t numDescriptors)
	{
		switch (rangeType)
		{
		case dx12::RootSignature::DescriptorType::CBV:
		{
			return bindPoint == 0 && numDescriptors == dx12::SysInfo::GetMaxDescriptorTableCBVs();
		}
		break;
		case dx12::RootSignature::DescriptorType::SRV:
		{
			return bindPoint == 0 && numDescriptors == dx12::SysInfo::GetMaxDescriptorTableSRVs();
		}
		break;
		case dx12::RootSignature::DescriptorType::UAV:
		{
			return bindPoint == 0 && numDescriptors == dx12::SysInfo::GetMaxDescriptorTableUAVs();
		}
		break;
		default: SEAssertF("Invalid range type");
		}
		return false; // This should never happen
	}


	void ValidateDescriptorRangeSizes(std::vector<dx12::RootSignature::DescriptorTable> const& tableMetadata)
	{
		SEStaticAssert(dx12::RootSignature::Type_Count == 3,
			"Root signature descriptor type count has changed. This function must be updated");

#if defined(_DEBUG)
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-support
		
		std::array<uint32_t, dx12::RootSignature::Type_Count> descriptorTypeCounts{}; // Value initializes as 0

		for (auto const& table : tableMetadata)
		{
			for (uint8_t descriptorTypeIdx = 0; descriptorTypeIdx < dx12::RootSignature::Type_Count; ++descriptorTypeIdx)
			{
				for (auto const& range : table.m_ranges[descriptorTypeIdx])
				{
					descriptorTypeCounts[descriptorTypeIdx] += range.m_bindCount;

					SEAssert(range.m_baseRegister != dx12::RootSignature::k_invalidRegisterVal,
						"Base register not initialized");

					SEAssert(range.m_registerSpace != dx12::RootSignature::k_invalidRegisterVal,
						"Register space not initialized");
				}
			}
		}

		SEAssert(descriptorTypeCounts[dx12::RootSignature::SRV] <= dx12::SysInfo::GetMaxDescriptorTableSRVs(),
			"More SRVs requested than allowed across all descriptor tables per shader stage");

		SEAssert(descriptorTypeCounts[dx12::RootSignature::UAV] <= dx12::SysInfo::GetMaxDescriptorTableUAVs(),
			"More UAVs requested than allowed across all descriptor tables per shader stage");

		SEAssert(descriptorTypeCounts[dx12::RootSignature::CBV] <= dx12::SysInfo::GetMaxDescriptorTableCBVs(),
			"More CBVs requested than allowed across all descriptor tables per shader stage");
#endif
	}
}

namespace dx12
{
	bool RootSignature::DescriptorTable::ContainsUnboundedArray() const
	{
		for (uint8_t rangeTypeIdx = 0; rangeTypeIdx < DescriptorType::Type_Count; ++rangeTypeIdx)
		{
			// We only need to check the first valid range entry to determine if the root index contains an
			// unbounded array
			if (m_ranges[rangeTypeIdx].empty() == false)
			{
				return IsUnboundedRange(
					static_cast<DescriptorType>(rangeTypeIdx),
					m_ranges[rangeTypeIdx][0].m_baseRegister,
					m_ranges[rangeTypeIdx][0].m_bindCount);
			}
		}
		return false;
	}


	// ---


	RootSignature::RootSignature()
		: m_rootSignature(nullptr)
		, m_rootSigDescHash(0)
		, m_rootSigDescriptorTableIdxBitmask(0)
		, m_isFinalized(false)
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

		m_rootParamMetadata.clear();
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
				rootParam.m_rootConstant.m_num32BitValues > 0 &&
				rootParam.m_rootConstant.m_num32BitValues <= 4),
			"Root constant entry is not correctly initialized");

		SEAssert(rootParam.m_type != RootParameter::Type::DescriptorTable || 
				(rootParam.m_tableEntry.m_type != DescriptorType::Type_Invalid &&
					rootParam.m_tableEntry.m_offset != k_invalidOffset && 
					(rootParam.m_tableEntry.m_type == DescriptorType::CBV ||
						rootParam.m_tableEntry.m_srvViewDimension != 0)), // It's a union, either member should be > 0
			"TableEntry is not fully initialized"); 

		const uint32_t metadataIdx = util::CheckedCast<uint32_t>(m_rootParamMetadata.size());

		// Map the name to the insertion index:
		SEAssert(!m_namesToRootParamsIdx.contains(name), "Name mapping metadata already exists");
		
		m_namesToRootParamsIdx.emplace(name, metadataIdx);

		// Finally, move the root param into our vector
		m_rootParamMetadata.emplace_back(std::move(rootParam));
	}


	void RootSignature::ParseInputBindingDesc(
		dx12::RootSignature* newRootSig, // Static function: Need our root sig object
		re::Shader::ShaderType shaderType,
		D3D12_SHADER_INPUT_BIND_DESC const& inputBindingDesc,
		std::array<std::vector<RangeInput>, DescriptorType::Type_Count>& rangeInputs,
		std::vector<CD3DX12_ROOT_PARAMETER1>& rootParameters,
		std::vector<std::string>& staticSamplerNames,
		std::vector<D3D12_STATIC_SAMPLER_DESC>& staticSamplers)
	{
		auto AddRangeInput = [&rangeInputs, &inputBindingDesc, &shaderType]
			(dx12::RootSignature::DescriptorType descriptorType)
			{
				uint32_t maxDescriptorCount = 0;
				switch (descriptorType)
				{
				case dx12::RootSignature::CBV: maxDescriptorCount = dx12::SysInfo::GetMaxDescriptorTableCBVs(); break;
				case dx12::RootSignature::SRV: maxDescriptorCount = dx12::SysInfo::GetMaxDescriptorTableSRVs(); break;
				case dx12::RootSignature::UAV: maxDescriptorCount = dx12::SysInfo::GetMaxDescriptorTableUAVs(); break;
				default: SEAssertF("Invalid descriptor type");
				}

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

					// Adjust bind counts for unbounded resoruce arrays to the max supported by the system
					if (newRangeInput.BindCount == 0 || // Bind count zero signals an unbounded array in a library shader
						newRangeInput.BindCount == std::numeric_limits<uint32_t>::max()) // Unbounded
					{
						newRangeInput.BindCount = maxDescriptorCount;
					}
				}
				else
				{
					SEAssert(result->BindPoint == inputBindingDesc.BindPoint &&
						result->Space == inputBindingDesc.Space &&
						result->Type == inputBindingDesc.Type &&
						(result->BindCount == inputBindingDesc.BindCount || 
							(result->BindCount == maxDescriptorCount && inputBindingDesc.BindCount == 0)) &&
						result->ReturnType == inputBindingDesc.ReturnType &&
						(result->Dimension == inputBindingDesc.Dimension ||
							(result->Dimension == D3D_SRV_DIMENSION_BUFFEREX && 
								inputBindingDesc.Dimension == D3D_SRV_DIMENSION_UNKNOWN)) &&
						result->NumSamples == inputBindingDesc.NumSamples,
						"Found resource with the same name but a different binding description");

					result->m_visibility = D3D12_SHADER_VISIBILITY_ALL;

					// Note: We update the visibility of the descriptor table later
				}
			};


		switch (inputBindingDesc.Type)
		{
		case D3D_SIT_RTACCELERATIONSTRUCTURE:
		{
			if (inputBindingDesc.BindCount == 1) // Single RT AS: Bind in the root signature
			{
				if (!newRootSig->m_namesToRootParamsIdx.contains(inputBindingDesc.Name))
				{
					const uint8_t rootIdx = util::CheckedCast<uint8_t>(rootParameters.size());
					CD3DX12_ROOT_PARAMETER1& newRootParam = rootParameters.emplace_back();

					constexpr D3D12_ROOT_DESCRIPTOR_FLAGS k_defaultASFlag = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
					constexpr D3D12_SHADER_VISIBILITY k_ASVisibility = D3D12_SHADER_VISIBILITY_ALL;

					newRootParam.InitAsShaderResourceView(
						inputBindingDesc.BindPoint,	// Shader register
						inputBindingDesc.Space,		// Register space
						k_defaultASFlag,			// Flags
						k_ASVisibility);			// Shader visibility

					newRootSig->InsertNewRootParamMetadata(
						inputBindingDesc.Name,
						RootParameter{
							.m_index = rootIdx,
							.m_type = RootParameter::Type::SRV,
							.m_registerBindPoint = inputBindingDesc.BindPoint,
							.m_registerSpace = inputBindingDesc.Space,
							.m_visibility = k_ASVisibility,
							.m_rootSRV{ 
								.m_viewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE,
								.m_flags = k_defaultASFlag,
							}
						});
				}
				// Note: AS is always visible, nothing to update if it already exists
			}
			else
			{
				AddRangeInput(dx12::RootSignature::DescriptorType::SRV);

				SEAssert(rangeInputs[dx12::RootSignature::DescriptorType::SRV].back().Dimension == D3D_SRV_DIMENSION_UNKNOWN ||
					rangeInputs[dx12::RootSignature::DescriptorType::SRV].back().Dimension == D3D_SRV_DIMENSION_BUFFEREX,
					"Unexpected dimension");

				// Shader reflection gives .Dimension = D3D_SRV_DIMENSION_UNKNOWN, switch it now so it's easier to get
				// the correct D3D12_SRV_DIMENSION (i.e. D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE) later on
				rangeInputs[dx12::RootSignature::DescriptorType::SRV].back().Dimension = D3D_SRV_DIMENSION_BUFFEREX;
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
			SEAssert(strcmp(inputBindingDesc.Name, "$Globals") != 0, "TODO: Handle global constants");
			
			if (inputBindingDesc.BindCount == 1)
			{
				if (!newRootSig->m_namesToRootParamsIdx.contains(inputBindingDesc.Name))
				{
					const uint8_t rootIdx = util::CheckedCast<uint8_t>(rootParameters.size());
					rootParameters.emplace_back();

					constexpr D3D12_ROOT_DESCRIPTOR_FLAGS k_defaultCBVFlag = 
						D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE; // Volatile = root sig 1.0 default
					
					const D3D12_SHADER_VISIBILITY visibility = GetShaderVisibilityFlagFromShaderType(shaderType);

					rootParameters[rootIdx].InitAsConstantBufferView(
						inputBindingDesc.BindPoint,	// Shader register
						inputBindingDesc.Space,		// Register space
						k_defaultCBVFlag,			// Flags
						visibility);				// Shader visibility

					newRootSig->InsertNewRootParamMetadata(inputBindingDesc.Name,
						RootParameter{
							.m_index = rootIdx,
							.m_type = RootParameter::Type::CBV,
							.m_registerBindPoint = inputBindingDesc.BindPoint,
							.m_registerSpace = inputBindingDesc.Space,
							.m_visibility = visibility,
							.m_rootCBV = RootCBV{
								.m_flags = k_defaultCBVFlag,
							}
						});
				}
				else
				{
					const uint32_t metadataIdx = newRootSig->m_namesToRootParamsIdx[inputBindingDesc.Name];
					rootParameters[newRootSig->m_rootParamMetadata[metadataIdx].m_index].ShaderVisibility =
						D3D12_SHADER_VISIBILITY_ALL;

					newRootSig->m_rootParamMetadata[metadataIdx].m_visibility = D3D12_SHADER_VISIBILITY_ALL;
				}
			}
			else
			{
				AddRangeInput(dx12::RootSignature::DescriptorType::CBV);
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
					D3D12_STATIC_SAMPLER_DESC const& samplerDesc = samplerPlatParams->m_staticSamplerDesc;
					return existing.Filter == samplerDesc.Filter &&
						existing.AddressU == samplerDesc.AddressU &&
						existing.AddressV == samplerDesc.AddressV &&
						existing.AddressW == samplerDesc.AddressW &&
						existing.MipLODBias == samplerDesc.MipLODBias &&
						existing.MaxAnisotropy == samplerDesc.MaxAnisotropy &&
						existing.ComparisonFunc == samplerDesc.ComparisonFunc &&
						existing.BorderColor == samplerDesc.BorderColor &&
						existing.MinLOD == samplerDesc.MinLOD &&
						existing.MaxLOD == samplerDesc.MaxLOD;
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

				staticSamplerNames.emplace_back(inputBindingDesc.Name);
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
			if (inputBindingDesc.BindCount == 1) // Single RWStructured buffer: Bind in the root signature
			{
				if (!newRootSig->m_namesToRootParamsIdx.contains(inputBindingDesc.Name))
				{
					const uint8_t rootIdx = util::CheckedCast<uint8_t>(rootParameters.size());
					rootParameters.emplace_back();

					constexpr D3D12_ROOT_DESCRIPTOR_FLAGS k_defaultRWStructuredFlag = 
						D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE; // Volatile = root sig 1.0 default

					const D3D12_SHADER_VISIBILITY visibility = GetShaderVisibilityFlagFromShaderType(shaderType);

					rootParameters[rootIdx].InitAsUnorderedAccessView(
						inputBindingDesc.BindPoint,	// Shader register
						inputBindingDesc.Space,		// Register space
						k_defaultRWStructuredFlag,	// Flags
						visibility);				// Shader visibility

					newRootSig->InsertNewRootParamMetadata(inputBindingDesc.Name,
						RootParameter{
							.m_index = rootIdx,
							.m_type = RootParameter::Type::UAV,
							.m_registerBindPoint = inputBindingDesc.BindPoint,
							.m_registerSpace = inputBindingDesc.Space,
							.m_visibility = visibility,
							.m_rootUAV{
								.m_viewDimension = GetD3D12UAVDimension(inputBindingDesc.Dimension),
								.m_flags = k_defaultRWStructuredFlag,
							}
						});
				}
				else
				{
					const uint32_t metadataIdx = newRootSig->m_namesToRootParamsIdx[inputBindingDesc.Name];
					rootParameters[newRootSig->m_rootParamMetadata[metadataIdx].m_index].ShaderVisibility =
						D3D12_SHADER_VISIBILITY_ALL;

					newRootSig->m_rootParamMetadata[metadataIdx].m_visibility = D3D12_SHADER_VISIBILITY_ALL;
				}
			}
			else // RWStructured buffer arrays: Bind as a range
			{
				AddRangeInput(dx12::RootSignature::DescriptorType::UAV);
			}
		}
		break;
		case D3D_SIT_STRUCTURED: // Structured buffer
		{			
			if (inputBindingDesc.BindCount == 1) // Single structured buffer: Bind in the root signature
			{
				if (!newRootSig->m_namesToRootParamsIdx.contains(inputBindingDesc.Name))
				{
					const uint8_t rootIdx = util::CheckedCast<uint8_t>(rootParameters.size());
					rootParameters.emplace_back();

					constexpr D3D12_ROOT_DESCRIPTOR_FLAGS k_defaultStructuredFlag = 
						D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE; // Volatile = root sig 1.0 default

					const D3D12_SHADER_VISIBILITY visibility = GetShaderVisibilityFlagFromShaderType(shaderType);

					rootParameters[rootIdx].InitAsShaderResourceView(
						inputBindingDesc.BindPoint,	// Shader register
						inputBindingDesc.Space,		// Register space
						k_defaultStructuredFlag,	// Flags
						visibility);				// Shader visibility

					newRootSig->InsertNewRootParamMetadata(inputBindingDesc.Name,
						RootParameter{
							.m_index = rootIdx,
							.m_type = RootParameter::Type::SRV,
							.m_registerBindPoint = inputBindingDesc.BindPoint,
							.m_registerSpace = inputBindingDesc.Space,
							.m_visibility = visibility,
							.m_rootSRV{
								.m_viewDimension = GetD3D12SRVDimension(inputBindingDesc.Dimension),
								.m_flags = k_defaultStructuredFlag,
							}
						});
				}
				else
				{
					const size_t metadataIdx = newRootSig->m_namesToRootParamsIdx[inputBindingDesc.Name];
					rootParameters[newRootSig->m_rootParamMetadata[metadataIdx].m_index].ShaderVisibility =
						D3D12_SHADER_VISIBILITY_ALL;

					newRootSig->m_rootParamMetadata[metadataIdx].m_visibility = D3D12_SHADER_VISIBILITY_ALL;
				}
			}
			else // Structured buffer arrays: Bind as a range
			{
				AddRangeInput(dx12::RootSignature::DescriptorType::SRV);
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


	void RootSignature::ParseTableRanges(
		dx12::RootSignature* newRootSig, // Static function: Need our root sig object
		std::array<std::vector<dx12::RootSignature::RangeInput>, DescriptorType::Type_Count> const& rangeInputs,
		std::vector<CD3DX12_ROOT_PARAMETER1>& rootParameters,
		std::vector<CD3DX12_DESCRIPTOR_RANGE1>& tableRanges)
	{
		// We're going to build a descriptor table that holds all of the range inputs:
		const uint8_t rootIdx = util::CheckedCast<uint8_t>(rootParameters.size());
		CD3DX12_ROOT_PARAMETER1& tableRootParam = rootParameters.emplace_back();
		
		uint32_t totalRangeDescriptors = 0; // How many descriptors in all ranges

		// Record the index of the element we're about to append:
		const size_t tableRangesBaseOffset = tableRanges.size();

		bool seenBounded = false; // Have we seen a bounded range?

		// TODO: Seperate ranges with different visibilities into different descriptor tables 
		bool seenFirstRangeVisibility = false;
		D3D12_SHADER_VISIBILITY tableVisibility = D3D12_SHADER_VISIBILITY_ALL;

		uint32_t descriptorOffset = 0;

		// Create a new DescriptorTable metadata entry:
		DescriptorTable* newDescriptorTable = &newRootSig->m_descriptorTables.emplace_back();;
		newDescriptorTable->m_index = rootIdx;

		for (size_t rangeTypeIdx = 0; rangeTypeIdx < DescriptorType::Type_Count; rangeTypeIdx++)
		{
			if (rangeInputs[rangeTypeIdx].size() == 0)
			{
				continue;
			}

			const DescriptorType rangeType = static_cast<DescriptorType>(rangeTypeIdx);

			// Walk through the sorted descriptors, and build ranges from contiguous blocks:
			size_t rangeStart = 0;
			size_t rangeEnd = 1;
			std::vector<std::string> namesInRange;

			// Get the least permissive shader visibility for the table as possible:
			if (!seenFirstRangeVisibility)
			{
				tableVisibility = rangeInputs[rangeType][rangeStart].m_visibility;

				seenFirstRangeVisibility = true;
			}
			else if (rangeInputs[rangeType][rangeStart].m_visibility != tableVisibility)
			{
				tableVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}

			while (rangeStart < rangeInputs[rangeType].size())
			{
				uint32_t maxRangeSize = 0;
				switch (rangeType)
				{
				case DescriptorType::CBV: maxRangeSize = SysInfo::GetMaxDescriptorTableCBVs(); break;
				case DescriptorType::SRV: maxRangeSize = SysInfo::GetMaxDescriptorTableSRVs(); break;
				case DescriptorType::UAV: maxRangeSize = SysInfo::GetMaxDescriptorTableUAVs(); break;
				default: SEAssertF("Invalid range type");
				}

				SEAssert(rangeInputs[rangeType][rangeStart].BindPoint == 0 ||
					rangeInputs[rangeType][rangeStart].BindCount != maxRangeSize,
					"Unbounded descriptor range doesn't begin at bind point 0. Indexing is about to overflow");

				// Store the names in order so we can update the binding metadata later:
				namesInRange.emplace_back(rangeInputs[rangeType][rangeStart].m_name);

				uint32_t numDescriptors = rangeInputs[rangeType][rangeStart].BindCount;
				uint32_t expectedNextRegister = rangeInputs[rangeType][rangeStart].BindPoint + numDescriptors;

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

				SEAssert(maxRangeSize - totalRangeDescriptors >= numDescriptors ||
					(totalRangeDescriptors == maxRangeSize && numDescriptors == maxRangeSize),
					"totalRangeDescriptors is about to overflow");

				if (totalRangeDescriptors != maxRangeSize)
				{
					totalRangeDescriptors += numDescriptors;
				}

				const uint32_t bindPoint = rangeInputs[rangeType][rangeStart].BindPoint;
				const uint32_t registerSpace = rangeInputs[rangeType][rangeStart].Space;

				constexpr D3D12_DESCRIPTOR_RANGE_FLAGS k_defaultRangeFlag =
					D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE; // Volatile = root sig 1.0 default

				const bool isUnbounded = IsUnboundedRange(rangeType, bindPoint, numDescriptors);

				seenBounded |= !isUnbounded;
				SEAssert(!seenBounded || !isUnbounded, 
					"Found bounded and unbounded descriptors in the same range inputs. These should have been seperated")

				// Create and initialize a CD3DX12_DESCRIPTOR_RANGE1:
				tableRanges.emplace_back().Init(
					GetD3DRangeType(rangeType),
					numDescriptors,
					bindPoint,
					registerSpace,
					k_defaultRangeFlag,
					isUnbounded ? 0 : D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

				// Populate the descriptor metadata:
				uint32_t baseRegisterOffset = 0; // We are processing contiguous ranges of registers only
				for (size_t rangeIdx = rangeStart; rangeIdx < rangeEnd; rangeIdx++)
				{
					const uint32_t registerBindPoint = bindPoint + baseRegisterOffset++;

					// Create the binding metadata for our individual RootParameter descriptor table entries:
					RootParameter rootParameter{
						.m_index = rootIdx,
						.m_type = RootParameter::Type::DescriptorTable,
						.m_registerBindPoint = registerBindPoint,
						.m_registerSpace = registerSpace,
						.m_visibility = tableVisibility,
						.m_tableEntry = RootSignature::TableEntry{
							.m_type = rangeType,
							.m_offset = isUnbounded ? 0 : descriptorOffset, // Descriptor offset into the table at the root index
							//.m_srv/uavViewDimension populated below
						}
					};
					descriptorOffset++;

					// Create a single metadata entry for the contiguous range of descriptors within DescriptorTables:
					bool isNewRange = false;
					if (rangeIdx == rangeStart ||
						rangeInputs[rangeType][rangeIdx].ReturnType != rangeInputs[rangeType][rangeStart].ReturnType ||
						rangeInputs[rangeType][rangeIdx].Dimension != rangeInputs[rangeType][rangeStart].Dimension)
					{
						isNewRange = true;
					}

					// Populate the descriptor table metadata:
					switch (rangeType)
					{
					case DescriptorType::CBV:
					{
						if (isNewRange)
						{
							newDescriptorTable->m_ranges[DescriptorType::CBV].emplace_back(RangeEntry{
								.m_bindCount = rangeInputs[rangeType][rangeIdx].BindCount,
								.m_baseRegister = registerBindPoint,
								.m_registerSpace = registerSpace,
								.m_flags = k_defaultRangeFlag,
								});
						}
						else
						{
							newDescriptorTable->m_ranges[DescriptorType::CBV].back().m_bindCount +=
								rangeInputs[rangeType][rangeIdx].BindCount;
						}
					}
					break;
					case DescriptorType::SRV:
					{
						const D3D12_SRV_DIMENSION d3d12SrvDimension =
							GetD3D12SRVDimension(rangeInputs[rangeType][rangeIdx].Dimension);

						rootParameter.m_tableEntry.m_srvViewDimension = d3d12SrvDimension;

						if (isNewRange)
						{
							newDescriptorTable->m_ranges[DescriptorType::SRV].emplace_back(RangeEntry{
								.m_bindCount = rangeInputs[rangeType][rangeIdx].BindCount,
								.m_baseRegister = registerBindPoint,
								.m_registerSpace = registerSpace,
								.m_flags = k_defaultRangeFlag,
								.m_srvDesc = {
									.m_format = GetFormatFromReturnType(rangeInputs[rangeType][rangeIdx].ReturnType),
									.m_viewDimension = d3d12SrvDimension,}
								});
						}
						else
						{
							newDescriptorTable->m_ranges[DescriptorType::SRV].back().m_bindCount += 
								rangeInputs[rangeType][rangeIdx].BindCount;
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
								.m_bindCount = rangeInputs[rangeType][rangeIdx].BindCount,
								.m_baseRegister = registerBindPoint,
								.m_registerSpace = registerSpace,
								.m_flags = k_defaultRangeFlag,
								.m_uavDesc = {
									.m_format = GetFormatFromReturnType(rangeInputs[rangeType][rangeIdx].ReturnType),
									.m_viewDimension = d3d12UavDimension,}
								});
						}
						else
						{
							newDescriptorTable->m_ranges[DescriptorType::UAV].back().m_bindCount +=
								rangeInputs[rangeType][rangeIdx].BindCount;
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

			} // rangeInputs loop
		} // End descriptor table DescriptorType loop


		// Now that we're done, set the visibility we determined on our descriptor table metadata:
		newDescriptorTable->m_visibility = tableVisibility;

		// Determine how many new ranges we've added in total:
		const uint32_t numDescriptorRanges = util::CheckedCast<uint32_t>(tableRanges.size() - tableRangesBaseOffset);

		// Initialize the root parameter as a descriptor table built from our ranges:
		tableRootParam.InitAsDescriptorTable(
			numDescriptorRanges,
			&tableRanges[tableRangesBaseOffset],
			tableVisibility);

		// How many descriptors are in the table stored at the given root sig index:
		newRootSig->m_numDescriptorsPerTable[rootIdx] = totalRangeDescriptors;

		const uint64_t descriptorTableBitmask = (1llu << rootIdx);
		newRootSig->m_rootSigDescriptorTableIdxBitmask |= descriptorTableBitmask;
	}


	std::unique_ptr<dx12::RootSignature> RootSignature::Create(re::Shader const& shader)
	{
		dx12::Shader::PlatformParams* shaderPlatParams = shader.GetPlatformParams()->As<dx12::Shader::PlatformParams*>();
		SEAssert(shaderPlatParams->m_isCreated, "Shader must be created");

		std::unique_ptr<dx12::RootSignature> newRootSig;
		newRootSig.reset(new dx12::RootSignature());

		// We build metadata about individual descriptors we'll place together into descriptor tables, and then use it
		// to build the tables later. Resource descriptors are grouped by type (CBV/SRV/UAV), then sorted into
		// contiguous ranges and packed together
		std::array<std::vector<RangeInput>, DescriptorType::Type_Count> rangeInputs;

		std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;
		staticSamplers.reserve(k_expectedNumberOfSamplers);

		std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters;
		rootParameters.reserve(k_maxRootSigEntries);

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
							newRootSig->m_staticSamplerNames,
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
						newRootSig->m_staticSamplerNames,
						staticSamplers);
				}
			}
		}


		// Isolate unbounded ranges, and combine them into a single root index:
		std::array<std::vector<RangeInput>, DescriptorType::Type_Count> unboundedRanges;
		std::array<std::vector<RangeInput>, DescriptorType::Type_Count> boundedRanges;
		bool hasUnboundedRange = false;
		bool hasBoundedRange = false;

		for (size_t rangeTypeIdx = 0; rangeTypeIdx < DescriptorType::Type_Count; rangeTypeIdx++)
		{
			// Sort the range entries by register value, so they can be packed contiguously
			std::sort(
				rangeInputs[rangeTypeIdx].begin(),
				rangeInputs[rangeTypeIdx].end(),
				[](auto const& a, auto const& b)
				{
					if (a.BindPoint == b.BindPoint)
					{
						SEAssert(a.Space != b.Space, "Register collision");
						return a.Space < b.Space;
					}
					return a.BindPoint < b.BindPoint;
				});

			// Separate unbounded ranges so we can assign them a unique root signature index
			for (auto& range : rangeInputs[rangeTypeIdx])
			{
				if (IsUnboundedRange(static_cast<DescriptorType>(rangeTypeIdx), range.BindPoint, range.BindCount))
				{
					unboundedRanges[rangeTypeIdx].emplace_back(range);
					hasUnboundedRange = true;
				}
				else
				{
					boundedRanges[rangeTypeIdx].emplace_back(range);
					hasBoundedRange = true;
				}
			}
		}

		// TODO: Sort rootParameters based on the .ParameterType, to ensure optimal/preferred ordering/grouping of 
		// entries stored directly in the root signature
		// - MS recommends binding the most frequently changing elements at the start of the root signature.
		//		- For SaberEngine, that's probably buffers: CBVs and SRVs
	
		std::vector<CD3DX12_DESCRIPTOR_RANGE1> tableRanges; // Must keep these in scope
		tableRanges.reserve(k_maxRootSigEntries * DescriptorType::Type_Count);

		if (hasBoundedRange)
		{
			ParseTableRanges(newRootSig.get(), boundedRanges, rootParameters, tableRanges);
		}
		if (hasUnboundedRange)
		{
			ParseTableRanges(newRootSig.get(), unboundedRanges, rootParameters, tableRanges);
		}

		SEAssert(tableRanges.size() <= k_maxRootSigEntries * DescriptorType::Type_Count,
			"Reallocation detected, internal pointers have been invalidated");

		// Allow/deny unnecessary shader access
		const D3D12_ROOT_SIGNATURE_FLAGS rootSigFlags = BuildRootSignatureFlags(shaderPlatParams->m_shaderBlobs);

		const std::wstring rootSigName = shader.GetWName();

		newRootSig->FinalizeInternal(rootSigName, rootParameters, staticSamplers, rootSigFlags);

		return newRootSig;
	}


	void RootSignature::ValidateRootSigSize()
	{
#if defined(_DEBUG)
		constexpr uint8_t k_descriptorTableCost = 1;	// 1 DWORD each
		constexpr uint8_t k_rootConstantCost = 1;		// 1 DWORD each
		constexpr uint8_t k_rootDescriptorCost = 2;		// 2 DWORDs each

		// Descriptor tables:
		uint8_t rootSigSize = util::CheckedCast<uint8_t>(m_descriptorTables.size()) * k_descriptorTableCost;

		// Everything else:
		for (auto const& param : m_rootParamMetadata)
		{
			switch (param.m_type)
			{
			case RootParameter::Type::Constant:
			{
				rootSigSize += k_rootConstantCost;
			}
			break;
			case RootParameter::Type::CBV:
			case RootParameter::Type::SRV:
			case RootParameter::Type::UAV:
			{
				rootSigSize += k_rootDescriptorCost;
			}
			break;
			case RootParameter::Type::DescriptorTable:
			{
				// Handled above
			}
			break;
			default: SEAssertF("Invalid descriptor type");
			}
		}

		SEAssert(rootSigSize <= 64, "A D3D root signature must be 64 DWORDs max");

#endif
	}


	void RootSignature::FinalizeInternal(
		std::wstring const& rootSigName,
		std::vector<CD3DX12_ROOT_PARAMETER1> const& rootParameters,
		std::vector<D3D12_STATIC_SAMPLER_DESC> const& staticSamplers,
		D3D12_ROOT_SIGNATURE_FLAGS rootSigFlags)
	{
		SEAssert(!m_isFinalized, "Root signature has already been finalized");

		ValidateDescriptorRangeSizes(m_descriptorTables); // _DEBUG only
		ValidateRootSigSize(); // _DEBUG only

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
			rootSigFlags);										// D3D12_ROOT_SIGNATURE_FLAGS

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		// Before we create a root signature, check if one with the same layout already exists:
		m_rootSigDescHash = HashRootSigDesc(rootSignatureDescription);
		if (context->HasRootSignature(m_rootSigDescHash))
		{
			m_rootSignature = context->GetRootSignature(m_rootSigDescHash);

			// Root signature is shared: Update the name
			std::wstring const& newName = L"Shared: " + dx12::GetWDebugName(m_rootSignature.Get()) + rootSigName.c_str();
			m_rootSignature->SetName(newName.c_str());
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
				IID_PPV_ARGS(&m_rootSignature));
			CheckHResult(hr, "Failed to create root signature");

			m_rootSignature->SetName(rootSigName.c_str());

			// Add the new root sig to the library:
			context->AddRootSignature(m_rootSigDescHash, m_rootSignature);
		}

		m_isFinalized = true;
	}


	std::unique_ptr<dx12::RootSignature> RootSignature::CreateUninitialized()
	{
		std::unique_ptr<dx12::RootSignature> newRootSig;
		newRootSig.reset(new dx12::RootSignature());

		return newRootSig;
	}


	uint32_t RootSignature::AddRootParameter(RootParameterCreateDesc const& rootParamDesc)
	{
		SEAssert(rootParamDesc.m_type != RootParameter::Type::DescriptorTable,
			"Invalid root parameter type: Use AddDescriptorTable() instead");

		SEAssert(!m_isFinalized, "Root signature has already been finalized");

		const uint8_t rootIndex = m_rootParamMetadata.empty() ? 0 : m_rootParamMetadata.back().m_index + 1;

		RootParameter newRootParam{
			.m_index = rootIndex,
			.m_type = rootParamDesc.m_type,
			.m_registerBindPoint = rootParamDesc.m_registerBindPoint,
			.m_registerSpace = rootParamDesc.m_registerSpace,
			.m_visibility = rootParamDesc.m_visibility,
		};

		// Union members:
		switch (rootParamDesc.m_type)
		{
		case RootParameter::Type::Constant:
		{
			newRootParam.m_rootConstant = RootConstant{
				.m_num32BitValues = util::CheckedCast<uint8_t>(rootParamDesc.m_numRootConstants),
			};
		}
		break;
		case RootParameter::Type::CBV:
		{
			newRootParam.m_rootCBV = RootCBV{
				.m_flags = rootParamDesc.m_flags,
			};
		}
		break;
		case RootParameter::Type::SRV:
		{
			newRootParam.m_rootSRV = RootSRV{
				.m_viewDimension = rootParamDesc.m_srvViewDimension,
				.m_flags = rootParamDesc.m_flags,
			};
		}
		break;
		case RootParameter::Type::UAV:
		{
			newRootParam.m_rootUAV = RootUAV{
				.m_viewDimension = rootParamDesc.m_uavViewDimension,
				.m_flags = rootParamDesc.m_flags,
			};
		}
		break;
		case RootParameter::Type::DescriptorTable:
		{
			//
		}
		break;
		default: SEAssertF("Invalid root parameter type");
		}

		InsertNewRootParamMetadata(rootParamDesc.m_shaderName.c_str(), std::move(newRootParam));

		return rootIndex;
	}


	uint32_t RootSignature::AddDescriptorTable(
		std::vector<DescriptorRangeCreateDesc> const& tableRanges,
		D3D12_SHADER_VISIBILITY visibility /*= D3D12_SHADER_VISIBILITY_ALL*/)
	{
		SEAssert(!m_isFinalized, "Root signature has already been finalized");

		const uint8_t rootIndex = m_rootParamMetadata.empty() ? 0 : m_rootParamMetadata.back().m_index + 1;

		DescriptorTable& descriptorTableMetadata = m_descriptorTables.emplace_back();
		descriptorTableMetadata.m_index = rootIndex;
		descriptorTableMetadata.m_visibility = visibility;

		uint32_t totalRangeDescriptors = 0;
		for (auto const& range : tableRanges)
		{
			D3D12_DESCRIPTOR_RANGE1 const& rangeDesc = range.m_rangeDesc;

			SEAssert((rangeDesc.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV &&
				rangeDesc.NumDescriptors <= dx12::SysInfo::GetMaxDescriptorTableCBVs()) ||
				(rangeDesc.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV &&
					rangeDesc.NumDescriptors <= dx12::SysInfo::GetMaxDescriptorTableSRVs()) ||
				(rangeDesc.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV &&
					rangeDesc.NumDescriptors <= dx12::SysInfo::GetMaxDescriptorTableUAVs()),
				"Too many descriptors for the current descriptor range type");

			totalRangeDescriptors += rangeDesc.NumDescriptors;

			const DescriptorType descriptorType = D3DDescriptorRangeTypeToDescriptorType(rangeDesc.RangeType);

			RootParameter rangeRootParam{
				.m_index = rootIndex,
				.m_type = RootParameter::Type::DescriptorTable,
				.m_registerBindPoint = rangeDesc.BaseShaderRegister,
				.m_registerSpace = rangeDesc.RegisterSpace,
				.m_visibility = visibility,
				.m_tableEntry = RootSignature::TableEntry{
					.m_type = descriptorType,
					.m_offset = util::CheckedCast<uint8_t>(rangeDesc.OffsetInDescriptorsFromTableStart),
					//.m_srv/uavViewDimension populated below
				}
			};

			// Populate the RangeEntry metadata:
			RangeEntry& rangeEntry = descriptorTableMetadata.m_ranges[descriptorType].emplace_back();
			
			rangeEntry.m_bindCount = rangeDesc.NumDescriptors;	
			rangeEntry.m_baseRegister = rangeDesc.BaseShaderRegister;
			rangeEntry.m_registerSpace = rangeDesc.RegisterSpace;
			rangeEntry.m_flags = rangeDesc.Flags;

			switch (descriptorType)
			{
			case DescriptorType::CBV:
			{
				//
			}
			break;
			case DescriptorType::SRV:
			{
				rangeEntry.m_srvDesc.m_format = range.m_srvDesc.m_format;
				rangeEntry.m_srvDesc.m_viewDimension = range.m_srvDesc.m_viewDimension;

				rangeRootParam.m_tableEntry.m_srvViewDimension = range.m_srvDesc.m_viewDimension;
			}
			break;
			case DescriptorType::UAV:
			{
				rangeEntry.m_uavDesc.m_format = range.m_uavDesc.m_format;
				rangeEntry.m_uavDesc.m_viewDimension = range.m_uavDesc.m_viewDimension;

				rangeRootParam.m_tableEntry.m_uavViewDimension = range.m_uavDesc.m_viewDimension;
			}
			break;
			default: SEAssertF("Invalid descriptor type");
			}

			// Record the root param metadata for the named resource:
			InsertNewRootParamMetadata(range.m_shaderName.c_str(), std::move(rangeRootParam));
		}

		// Update the descriptor table bitmasks:
		m_numDescriptorsPerTable[rootIndex] = totalRangeDescriptors;

		const uint64_t descriptorTableBitmask = (1llu << rootIndex);
		m_rootSigDescriptorTableIdxBitmask |= descriptorTableBitmask;

		return rootIndex;
	}


	void RootSignature::AddStaticSampler(core::InvPtr<re::Sampler> const& sampler)
	{
		SEAssert(!m_isFinalized, "Root signature has already been finalized");

		SEAssert(std::find(
			m_staticSamplerNames.begin(), 
			m_staticSamplerNames.end(), 
			sampler->GetName()) == m_staticSamplerNames.end(),
			"Sampler already added");
		
		m_staticSamplerNames.emplace_back(sampler->GetName());
	}

	
	void RootSignature::Finalize(char const* name, D3D12_ROOT_SIGNATURE_FLAGS rootSigFlags)
	{
		// Count the number of unique root signature indices we'll be populating:
		uint32_t numRootSigEntries = 0;
		for (size_t i = 0; i < m_rootParamMetadata.size(); ++i)
		{
			SEAssert(i == 0 || m_rootParamMetadata[i].m_index >= m_rootParamMetadata[i - 1].m_index,
				"Root parameter metadata is not stored in monotonically-increasing order");

			// Each named resource stored in a descriptor table has a unique entry in m_rootParamMetadata; Just count
			// unique indices
			if (i == 0 || // 1st iteration
				m_rootParamMetadata[i].m_index > m_rootParamMetadata[i - 1].m_index)
			{
				numRootSigEntries++;
			}
		}
		SEAssert(numRootSigEntries > 0, "No root signature entries. This is unexpected");

		// Build our list of root signature parameters from the recorded metadata:
		std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters;
		rootParameters.resize(numRootSigEntries);

		for (auto const& rootParam : m_rootParamMetadata)
		{
			switch (rootParam.m_type)
			{
				case RootParameter::Type::Constant:
				{
					rootParameters[rootParam.m_index].InitAsConstants(
						rootParam.m_rootConstant.m_num32BitValues, 
						rootParam.m_registerBindPoint, 
						rootParam.m_registerSpace,
						rootParam.m_visibility);
				}
				break;
				case RootParameter::Type::CBV:
				{
					rootParameters[rootParam.m_index].InitAsConstantBufferView(
						rootParam.m_registerBindPoint,
						rootParam.m_registerSpace,
						rootParam.m_rootCBV.m_flags,
						rootParam.m_visibility);
				}
				break;
				case RootParameter::Type::SRV:
				{
					rootParameters[rootParam.m_index].InitAsShaderResourceView(
						rootParam.m_registerBindPoint,
						rootParam.m_registerSpace,
						rootParam.m_rootSRV.m_flags,
						rootParam.m_visibility);
				}
				break;
				case RootParameter::Type::UAV:
				{
					rootParameters[rootParam.m_index].InitAsUnorderedAccessView(
						rootParam.m_registerBindPoint,
						rootParam.m_registerSpace,
						rootParam.m_rootUAV.m_flags,
						rootParam.m_visibility);
				}
				break;
				case RootParameter::Type::DescriptorTable:
				{
					// We'll handle these at the end
				}
				break;
				default: SEAssertF("Invalid root parameter type");
			}
		}

		std::vector<std::vector<D3D12_DESCRIPTOR_RANGE1>> allDescriptorRanges; // Keep these in scope until we're done
		allDescriptorRanges.reserve(m_descriptorTables.size() * DescriptorType::Type_Count);

		// Initialize rootParameters containing descriptor tables:
		for (DescriptorTable const& tableMetadata : m_descriptorTables)
		{
			std::vector<D3D12_DESCRIPTOR_RANGE1>& tableRanges = allDescriptorRanges.emplace_back();

			for (uint8_t rangeTypeIdx = 0; rangeTypeIdx < DescriptorType::Type_Count; ++rangeTypeIdx)
			{
				for (RangeEntry const& rangeEntry : tableMetadata.m_ranges[rangeTypeIdx])
				{
					const bool isUnboundedRange = IsUnboundedRange(
						static_cast<DescriptorType>(rangeTypeIdx), rangeEntry.m_baseRegister, rangeEntry.m_bindCount);

					D3D12_DESCRIPTOR_RANGE1& descriptorRange = tableRanges.emplace_back();

					descriptorRange.RangeType = GetD3DRangeType(static_cast<DescriptorType>(rangeTypeIdx));
					descriptorRange.NumDescriptors = rangeEntry.m_bindCount;
					descriptorRange.BaseShaderRegister = rangeEntry.m_baseRegister;
					descriptorRange.RegisterSpace = rangeEntry.m_registerSpace;
					descriptorRange.Flags = rangeEntry.m_flags;
					descriptorRange.OffsetInDescriptorsFromTableStart = 
						isUnboundedRange ? 0 : D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				}
			}

			rootParameters[tableMetadata.m_index].InitAsDescriptorTable(
				util::CheckedCast<uint32_t>(tableRanges.size()),
				tableRanges.data());
		}


		// Static samplers:
		std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;
		staticSamplers.reserve(k_expectedNumberOfSamplers);

		for (auto const& samplerName : m_staticSamplerNames)
		{
			core::InvPtr<re::Sampler> const& sampler = re::Sampler::GetSampler(samplerName);

			dx12::Sampler::PlatformParams* samplerPlatParams =
				sampler->GetPlatformParams()->As<dx12::Sampler::PlatformParams*>();

			staticSamplers.emplace_back(samplerPlatParams->m_staticSamplerDesc);
		}
		SEAssert(staticSamplers.size() <= 2032,
			"The maximum number of unique static samplers across live root signatures is 2032 (+16 reserved "
			"for drivers that need their own samplers)");

		// Lastly, finalize the root sig:
		std::wstring const& rootSigName = util::ToWideString(name);

		FinalizeInternal(rootSigName, rootParameters, staticSamplers, rootSigFlags);
	}


	RootSignature::RootParameter const* RootSignature::GetRootSignatureEntry(std::string const& resourceName) const
	{
		auto const& result = m_namesToRootParamsIdx.find(resourceName);
		const bool hasResource = result != m_namesToRootParamsIdx.end();

		SEAssert(hasResource || 
			core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
			"Root signature does not contain a parameter with that name");

		return hasResource ? &m_rootParamMetadata[result->second] : nullptr;
	}


	bool RootSignature::RootIndexContainsUnboundedArray(uint8_t rootIdx) const
	{
		const uint64_t descriptorTableBitmask = (1llu << rootIdx);
		const bool isDescriptorTable = (m_rootSigDescriptorTableIdxBitmask & descriptorTableBitmask);

		if (isDescriptorTable)
		{
			for (auto const& table : m_descriptorTables)
			{
				if (table.m_index == rootIdx)
				{
					for (uint8_t rangeTypeIdx = 0; rangeTypeIdx < DescriptorType::Type_Count; ++rangeTypeIdx)
					{
						return table.ContainsUnboundedArray();
					}
					SEAssertF("Found a table where all ranges are empty");
				}
			}
		}
		return false;

		// TODO: JUST STORE AN EXTRA BITMASK INSTEAD OF SEARCHING EACH TIME !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
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