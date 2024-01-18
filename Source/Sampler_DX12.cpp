// © 2022 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "RenderManager.h"
#include "RootSignature_DX12.h"
#include "Sampler_DX12.h"


namespace
{
	D3D12_TEXTURE_ADDRESS_MODE GetD3DAddressMode(re::Sampler::AddressMode addressMode)
	{
		switch (addressMode)
		{
		case re::Sampler::AddressMode::Wrap:
		{
			return D3D12_TEXTURE_ADDRESS_MODE::D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		}
		break;
		case re::Sampler::AddressMode::Mirror:
		{
			return D3D12_TEXTURE_ADDRESS_MODE::D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		}
		break;
		case re::Sampler::AddressMode::MirrorOnce:
		{
			return D3D12_TEXTURE_ADDRESS_MODE::D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
		}
		break;
		case re::Sampler::AddressMode::Clamp:
		{
			return D3D12_TEXTURE_ADDRESS_MODE::D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		}
		break;
		case re::Sampler::AddressMode::Border:
		{
			return D3D12_TEXTURE_ADDRESS_MODE::D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		}
		break;
		default:
			SEAssertF("Invalid address mode");
		}
		return D3D12_TEXTURE_ADDRESS_MODE::D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}


	D3D12_FILTER GetD3DFilterMode(re::Sampler::MinFilter minFilter, re::Sampler::MagFilter maxFilter)
	{
		switch (minFilter)
		{
		case re::Sampler::MinFilter::Nearest:
		{
			switch (maxFilter)
			{
			case re::Sampler::MagFilter::Nearest:
			{
				return D3D12_FILTER_MIN_MAG_MIP_POINT;
			}
			break;
			case re::Sampler::MagFilter::Linear:
			{
				return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
			}
			break;
			default:
				SEAssertF("Invalid MagFilter type");
			}
		}
		break;
		case re::Sampler::MinFilter::NearestMipMapLinear:
		{
			switch (maxFilter)
			{
			case re::Sampler::MagFilter::Nearest:
			{
				return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
			}
			break;
			case re::Sampler::MagFilter::Linear:
			{
				return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
			}
			break;
			default:
				SEAssertF("Invalid MagFilter type");
			}
		}
		break;
		case re::Sampler::MinFilter::Linear:
		{
			switch (maxFilter)
			{
			case re::Sampler::MagFilter::Nearest:
			{
				return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
			}
			break;
			case re::Sampler::MagFilter::Linear:
			{
				return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			}
			break;
			default:
				SEAssertF("Invalid MagFilter type");
			}
		}
		break;
		case re::Sampler::MinFilter::LinearMipMapLinear:
		{
			switch (maxFilter)
			{
			case re::Sampler::MagFilter::Nearest:
			{
				return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
			}
			break;
			case re::Sampler::MagFilter::Linear:
			{
				return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			}
			break;
			default:
				SEAssertF("Invalid MagFilter type");
			}
		}
		break;
		default:
			SEAssertF("Invalid MinFilterType");
		}
		return D3D12_FILTER_MIN_MAG_MIP_LINEAR; // Return a reasonable default to suppress compiler warning
	}
}


namespace dx12
{
	void Sampler::Create(re::Sampler& sampler)
	{
		dx12::Sampler::PlatformParams* samplerPlatParams =
			sampler.GetPlatformParams()->As<dx12::Sampler::PlatformParams*>();
		SEAssert(samplerPlatParams->m_isCreated == false, "Sampler is already created");
		samplerPlatParams->m_isCreated = true;

		re::Sampler::SamplerParams const& samplerParams = sampler.GetSamplerParams();
		SEAssert(samplerParams.m_maxAnisotropy >= 1 && samplerParams.m_maxAnisotropy <= 16, "Invalid max anisotropy");

		// We initialize a D3D12_STATIC_SAMPLER_DESC here for (re)use when creating root signatures
		samplerPlatParams->m_staticSamplerDesc.Filter =
			GetD3DFilterMode(samplerParams.m_texMinMode, samplerParams.m_texMagMode);

		// TODO: Support individual U/V/W address modes
		samplerPlatParams->m_staticSamplerDesc.AddressU = GetD3DAddressMode(samplerParams.m_addressMode);
		samplerPlatParams->m_staticSamplerDesc.AddressV = GetD3DAddressMode(samplerParams.m_addressMode);
		samplerPlatParams->m_staticSamplerDesc.AddressW = GetD3DAddressMode(samplerParams.m_addressMode);

		samplerPlatParams->m_staticSamplerDesc.MipLODBias = samplerParams.m_mipLODBias;
		samplerPlatParams->m_staticSamplerDesc.MaxAnisotropy = samplerParams.m_maxAnisotropy;

		// TODO: Support comparison functions
		samplerPlatParams->m_staticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		// TODO: Support variable colors?
		samplerPlatParams->m_staticSamplerDesc.BorderColor 
			= D3D12_STATIC_BORDER_COLOR::D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;

		samplerPlatParams->m_staticSamplerDesc.MinLOD = 0;
		samplerPlatParams->m_staticSamplerDesc.MaxLOD = std::numeric_limits<float>::max(); // TODO: Support this. For now, no limit

		// These params are set per-root signature, during root signature creation:
		samplerPlatParams->m_staticSamplerDesc.ShaderRegister = dx12::RootSignature::k_invalidRegisterVal;
		samplerPlatParams->m_staticSamplerDesc.RegisterSpace = dx12::RootSignature::k_invalidRegisterVal;
		samplerPlatParams->m_staticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL;
	}


	void Sampler::Destroy(re::Sampler& sampler)
	{
		dx12::Sampler::PlatformParams* samplerPlatParams =
			sampler.GetPlatformParams()->As<dx12::Sampler::PlatformParams*>();
		SEAssert(samplerPlatParams->m_isCreated == true, "Sampler has not been created");
		samplerPlatParams->m_isCreated = false;
	}
}