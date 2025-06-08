// © 2022 Adam Badke. All rights reserved.
#include "Private/Context_DX12.h"
#include "Private/RenderManager.h"
#include "Private/RootSignature_DX12.h"
#include "Private/Sampler_DX12.h"


namespace
{
	constexpr D3D12_FILTER GetD3DFilterMode(re::Sampler::FilterMode filterMode)
	{
		switch (filterMode)
		{
			case re::Sampler::FilterMode::MIN_MAG_MIP_POINT: return D3D12_FILTER_MIN_MAG_MIP_POINT;
			case re::Sampler::FilterMode::MIN_MAG_POINT_MIP_LINEAR: return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
			case re::Sampler::FilterMode::MIN_POINT_MAG_LINEAR_MIP_POINT: return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
			case re::Sampler::FilterMode::MIN_POINT_MAG_MIP_LINEAR: return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
			case re::Sampler::FilterMode::MIN_LINEAR_MAG_MIP_POINT: return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
			case re::Sampler::FilterMode::MIN_LINEAR_MAG_POINT_MIP_LINEAR: return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
			case re::Sampler::FilterMode::MIN_MAG_LINEAR_MIP_POINT: return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			case re::Sampler::FilterMode::MIN_MAG_MIP_LINEAR: return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			case re::Sampler::FilterMode::MIN_MAG_ANISOTROPIC_MIP_POINT: return D3D12_FILTER_MIN_MAG_ANISOTROPIC_MIP_POINT;
			case re::Sampler::FilterMode::ANISOTROPIC: return D3D12_FILTER_ANISOTROPIC;
			case re::Sampler::FilterMode::COMPARISON_MIN_MAG_MIP_POINT: return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
			case re::Sampler::FilterMode::COMPARISON_MIN_MAG_POINT_MIP_LINEAR: return D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
			case re::Sampler::FilterMode::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT: return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
			case re::Sampler::FilterMode::COMPARISON_MIN_POINT_MAG_MIP_LINEAR: return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
			case re::Sampler::FilterMode::COMPARISON_MIN_LINEAR_MAG_MIP_POINT: return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
			case re::Sampler::FilterMode::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR: return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
			case re::Sampler::FilterMode::COMPARISON_MIN_MAG_LINEAR_MIP_POINT: return D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
			case re::Sampler::FilterMode::COMPARISON_MIN_MAG_MIP_LINEAR: return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
			case re::Sampler::FilterMode::COMPARISON_MIN_MAG_ANISOTROPIC_MIP_POINT: return D3D12_FILTER_COMPARISON_MIN_MAG_ANISOTROPIC_MIP_POINT;
			case re::Sampler::FilterMode::COMPARISON_ANISOTROPIC: return D3D12_FILTER_COMPARISON_ANISOTROPIC;
			case re::Sampler::FilterMode::MINIMUM_MIN_MAG_MIP_POINT: return D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT;
			case re::Sampler::FilterMode::MINIMUM_MIN_MAG_POINT_MIP_LINEAR: return D3D12_FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR;
			case re::Sampler::FilterMode::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT: return D3D12_FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT;
			case re::Sampler::FilterMode::MINIMUM_MIN_POINT_MAG_MIP_LINEAR: return D3D12_FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR;
			case re::Sampler::FilterMode::MINIMUM_MIN_LINEAR_MAG_MIP_POINT: return D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT;
			case re::Sampler::FilterMode::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR: return D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
			case re::Sampler::FilterMode::MINIMUM_MIN_MAG_LINEAR_MIP_POINT: return D3D12_FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT;
			case re::Sampler::FilterMode::MINIMUM_MIN_MAG_MIP_LINEAR: return D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR;
			case re::Sampler::FilterMode::MINIMUM_MIN_MAG_ANISOTROPIC_MIP_POINT: return D3D12_FILTER_MINIMUM_MIN_MAG_ANISOTROPIC_MIP_POINT;
			case re::Sampler::FilterMode::MINIMUM_ANISOTROPIC: return D3D12_FILTER_MINIMUM_ANISOTROPIC;
			case re::Sampler::FilterMode::MAXIMUM_MIN_MAG_MIP_POINT: return D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT;
			case re::Sampler::FilterMode::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR: return D3D12_FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR;
			case re::Sampler::FilterMode::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT: return D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT;
			case re::Sampler::FilterMode::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR: return D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR;
			case re::Sampler::FilterMode::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT: return D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT;
			case re::Sampler::FilterMode::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR: return D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
			case re::Sampler::FilterMode::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT: return D3D12_FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT;
			case re::Sampler::FilterMode::MAXIMUM_MIN_MAG_MIP_LINEAR: return D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR;
			case re::Sampler::FilterMode::MAXIMUM_MIN_MAG_ANISOTROPIC_MIP_POINT: return D3D12_FILTER_MAXIMUM_MIN_MAG_ANISOTROPIC_MIP_POINT;
			case re::Sampler::FilterMode::MAXIMUM_ANISOTROPIC: return D3D12_FILTER_MAXIMUM_ANISOTROPIC;
		}
		return D3D12_FILTER_MIN_MAG_MIP_LINEAR; // Return a reasonable default to suppress compiler warning
	}


	constexpr D3D12_TEXTURE_ADDRESS_MODE GetD3DAddressMode(re::Sampler::EdgeMode edgeMode)
	{
		switch (edgeMode)
		{
		case re::Sampler::EdgeMode::Wrap: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		case re::Sampler::EdgeMode::Mirror: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		case re::Sampler::EdgeMode::MirrorOnce: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
		case re::Sampler::EdgeMode::Clamp: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		case re::Sampler::EdgeMode::Border: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		}
		return D3D12_TEXTURE_ADDRESS_MODE::D3D12_TEXTURE_ADDRESS_MODE_WRAP; // Suppress compiler warning
	}


	constexpr D3D12_COMPARISON_FUNC GetD3DComparisonFunc(re::Sampler::ComparisonFunc comparisonFunc)
	{
		switch (comparisonFunc)
		{
		case re::Sampler::ComparisonFunc::None: return D3D12_COMPARISON_FUNC_NONE;
		case re::Sampler::ComparisonFunc::Never: return D3D12_COMPARISON_FUNC_NEVER;
		case re::Sampler::ComparisonFunc::Less: return D3D12_COMPARISON_FUNC_LESS;
		case re::Sampler::ComparisonFunc::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
		case re::Sampler::ComparisonFunc::LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case re::Sampler::ComparisonFunc::Greater: return D3D12_COMPARISON_FUNC_GREATER;
		case re::Sampler::ComparisonFunc::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
		case re::Sampler::ComparisonFunc::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		case re::Sampler::ComparisonFunc::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
		}
		return D3D12_COMPARISON_FUNC_NONE; // Suppress compiler warning
	}


	constexpr D3D12_STATIC_BORDER_COLOR GetD3DBorderColor(re::Sampler::BorderColor borderColor)
	{
		switch (borderColor)
		{
		case re::Sampler::BorderColor::TransparentBlack: return D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		case re::Sampler::BorderColor::OpaqueBlack: return D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		case re::Sampler::BorderColor::OpaqueWhite: return D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
		case re::Sampler::BorderColor::OpaqueBlack_UInt: return D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK_UINT;
		case re::Sampler::BorderColor::OpaqueWhite_UInt: return D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE_UINT;
		}
		return D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	}
}


namespace dx12
{
	void Sampler::Create(re::Sampler& sampler)
	{
		dx12::Sampler::PlatObj* samplerPlatObj =
			sampler.GetPlatformObject()->As<dx12::Sampler::PlatObj*>();
		SEAssert(samplerPlatObj->m_isCreated == false, "Sampler is already created");
		samplerPlatObj->m_isCreated = true;

		re::Sampler::SamplerDesc const& samplerDesc = sampler.GetSamplerDesc();
		
		SEAssert(samplerDesc.m_maxAnisotropy >= 1 && samplerDesc.m_maxAnisotropy <= 16, "Invalid max anisotropy");

		// Populate our D3D12_STATIC_SAMPLER_DESC from our SE SamplerDesc:
		D3D12_STATIC_SAMPLER_DESC& staticSamplerDesc = samplerPlatObj->m_staticSamplerDesc;

		staticSamplerDesc.Filter = GetD3DFilterMode(samplerDesc.m_filterMode);

		staticSamplerDesc.AddressU = GetD3DAddressMode(samplerDesc.m_edgeModeU);
		staticSamplerDesc.AddressV = GetD3DAddressMode(samplerDesc.m_edgeModeV);
		staticSamplerDesc.AddressW = GetD3DAddressMode(samplerDesc.m_edgeModeW);

		staticSamplerDesc.MipLODBias = samplerDesc.m_mipLODBias;
		staticSamplerDesc.MaxAnisotropy = samplerDesc.m_maxAnisotropy;

		staticSamplerDesc.ComparisonFunc = GetD3DComparisonFunc(samplerDesc.m_comparisonFunc);

		staticSamplerDesc.BorderColor = GetD3DBorderColor(samplerDesc.m_borderColor);

		staticSamplerDesc.MinLOD = samplerDesc.m_minLOD;
		staticSamplerDesc.MaxLOD = samplerDesc.m_maxLOD;

		// These params are set per-root signature, during root signature creation:
		staticSamplerDesc.ShaderRegister = dx12::RootSignature::k_invalidRegisterVal;
		staticSamplerDesc.RegisterSpace = dx12::RootSignature::k_invalidRegisterVal;
		staticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL;
	}


	void Sampler::Destroy(re::Sampler& sampler)
	{
		dx12::Sampler::PlatObj* samplerPlatObj =
			sampler.GetPlatformObject()->As<dx12::Sampler::PlatObj*>();
		SEAssert(samplerPlatObj->m_isCreated == true, "Sampler has not been created");
		samplerPlatObj->m_isCreated = false;
	}
}