// © 2022 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "RenderManager.h"
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


	D3D12_FILTER GetD3DMinFilterMode(re::Sampler::MinFilter minFilter, re::Sampler::MaxFilter maxFilter)
	{
		switch (minFilter)
		{
		case re::Sampler::MinFilter::Nearest:
		{
			switch (maxFilter)
			{
			case re::Sampler::MaxFilter::Nearest:
			{
				return D3D12_FILTER_MIN_MAG_MIP_POINT;
			}
			break;
			case re::Sampler::MaxFilter::Linear:
			{
				return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
			}
			break;
			default:
				SEAssertF("Invalid MaxFilter type");
			}
		}
		break;
		case re::Sampler::MinFilter::NearestMipMapLinear:
		{
			switch (maxFilter)
			{
			case re::Sampler::MaxFilter::Nearest:
			{
				return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
			}
			break;
			case re::Sampler::MaxFilter::Linear:
			{
				return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
			}
			break;
			default:
				SEAssertF("Invalid MaxFilter type");
			}
		}
		break;
		case re::Sampler::MinFilter::Linear:
		{
			switch (maxFilter)
			{
			case re::Sampler::MaxFilter::Nearest:
			{
				return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
			}
			break;
			case re::Sampler::MaxFilter::Linear:
			{
				return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			}
			break;
			default:
				SEAssertF("Invalid MaxFilter type");
			}
		}
		break;
		case re::Sampler::MinFilter::LinearMipMapLinear:
		{
			switch (maxFilter)
			{
			case re::Sampler::MaxFilter::Nearest:
			{
				return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
			}
			break;
			case re::Sampler::MaxFilter::Linear:
			{
				return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			}
			break;
			default:
				SEAssertF("Invalid MaxFilter type");
			}
		}
		break;
		default:
			SEAssertF("Invalid MinFilterType");
		}
	}
}


namespace dx12
{
	Sampler::PlatformParams::PlatformParams(re::Sampler::SamplerParams const& samplerParams)
	{
		SEAssert("Invalid max anisotropy", samplerParams.m_maxAnisotropy >= 1 && samplerParams.m_maxAnisotropy <= 16);

		////dx12::Context::PlatformParams* ctxPlatParams =
		////	re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		////samplerPlatParams->m_cpuDescAllocation = std::move(
		////	ctxPlatParams->m_cpuDescriptorHeapMgrs[dx12::Context::CPUDescriptorHeapType::Sampler].Allocate(1));

		//// We initialize a D3D12_STATIC_SAMPLER_DESC here for (re)use when creating root signatures
		//m_staticSamplerDesc.Filter =
		//	GetD3DMinFilterMode(samplerParams.m_texMinMode, samplerParams.m_texMaxMode);
		//m_staticSamplerDesc.AddressU = GetD3DAddressMode(samplerParams.m_addressMode);
		//m_staticSamplerDesc.AddressV = GetD3DAddressMode(samplerParams.m_addressMode);
		//m_staticSamplerDesc.AddressW = GetD3DAddressMode(samplerParams.m_addressMode);
		//m_staticSamplerDesc.MipLODBias = samplerParams.m_mipLODBias;
		//m_staticSamplerDesc.MaxAnisotropy = samplerParams.m_maxAnisotropy;
		//m_staticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS; // TODO: Support this
		//m_staticSamplerDesc.BorderColor =
		//	D3D12_STATIC_BORDER_COLOR::D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK; // TODO: Support varialbe colors?

		///*memcpy(samplerDesc.BorderColor, &samplerPlatParams->m_borderColor.x, sizeof(samplerPlatParams->m_borderColor));*/

		//m_staticSamplerDesc.MinLOD = 0;
		//m_staticSamplerDesc.MaxLOD = std::numeric_limits<float>::max(); // TODO: Support this. For now, no limit

		//// These params are default initialized. Later, they're set per-root signature, during root signature creation
		//m_staticSamplerDesc.ShaderRegister = 0;
		//m_staticSamplerDesc.RegisterSpace = 0;
		//m_staticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL;
	}


	void Sampler::Create(re::Sampler& sampler)
	{
		dx12::Sampler::PlatformParams* samplerPlatParams =
			sampler.GetPlatformParams()->As<dx12::Sampler::PlatformParams*>();
		SEAssert("Sampler is already created", samplerPlatParams->m_isCreated == false);
		samplerPlatParams->m_isCreated = true;


		

		//re::Sampler::SamplerParams const& samplerParams = sampler.GetSamplerParams();
		//SEAssert("Invalid max anisotropy", samplerParams.m_maxAnisotropy >= 1 && samplerParams.m_maxAnisotropy <= 16);

		////dx12::Context::PlatformParams* ctxPlatParams =
		////	re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		////samplerPlatParams->m_cpuDescAllocation = std::move(
		////	ctxPlatParams->m_cpuDescriptorHeapMgrs[dx12::Context::CPUDescriptorHeapType::Sampler].Allocate(1));

		//// We initialize a D3D12_STATIC_SAMPLER_DESC here for (re)use when creating root signatures
		//samplerPlatParams->m_staticSamplerDesc.Filter =
		//	GetD3DMinFilterMode(samplerParams.m_texMinMode, samplerParams.m_texMaxMode);
		//samplerPlatParams->m_staticSamplerDesc.AddressU = GetD3DAddressMode(samplerParams.m_addressMode);
		//samplerPlatParams->m_staticSamplerDesc.AddressV = GetD3DAddressMode(samplerParams.m_addressMode);
		//samplerPlatParams->m_staticSamplerDesc.AddressW = GetD3DAddressMode(samplerParams.m_addressMode);
		//samplerPlatParams->m_staticSamplerDesc.MipLODBias = samplerParams.m_mipLODBias;
		//samplerPlatParams->m_staticSamplerDesc.MaxAnisotropy = samplerParams.m_maxAnisotropy;
		//samplerPlatParams->m_staticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS; // TODO: Support this
		//samplerPlatParams->m_staticSamplerDesc.BorderColor =
		//	D3D12_STATIC_BORDER_COLOR::D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK; // TODO: Support varialbe colors?

		///*memcpy(samplerDesc.BorderColor, &samplerPlatParams->m_borderColor.x, sizeof(samplerPlatParams->m_borderColor));*/

		//samplerPlatParams->m_staticSamplerDesc.MinLOD = 0;
		//samplerPlatParams->m_staticSamplerDesc.MaxLOD = std::numeric_limits<float>::max(); // TODO: Support this. For now, no limit

		//// These params are default initialized. Later, they're set per-root signature, during root signature creation
		//samplerPlatParams->m_staticSamplerDesc.ShaderRegister = 0;
		//samplerPlatParams->m_staticSamplerDesc.RegisterSpace = 0;
		//samplerPlatParams->m_staticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL;

		////ctxPlatParams->m_device.GetD3DDisplayDevice()->CreateSampler(
		////	&samplerDesc, 
		////	samplerPlatParams->m_cpuDescAllocation.GetBaseDescriptor());


		//// TODO: This could all be moved to the PlatformParams ctor, similar to the OpenGL implementation...
		
	}


	void Sampler::Destroy(re::Sampler& sampler)
	{
		#pragma message("TODO: Implement dx12::Sampler::Destroy")
	}
}