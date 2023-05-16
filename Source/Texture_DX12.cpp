// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Config.h"
#include "Context_DX12.h"
#include "DebugConfiguration.h"
#include "RenderManager_DX12.h"
#include "SwapChain_DX12.h"
#include "Texture_DX12.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	DXGI_FORMAT Texture::GetTextureFormat(re::Texture::TextureParams const& texParams)
	{	
		switch (texParams.m_format)
		{
			case re::Texture::Format::RGBA32F: // 32 bits per channel x N channels
			{
				return DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;
			}
			break;
			case re::Texture::Format::RGB32F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT;
			}
			break;
			case re::Texture::Format::RG32F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT;
			}
			break;
			case re::Texture::Format::R32F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;
			}
			break;
			case re::Texture::Format::RGBA16F: // 16 bits per channel x N channels
			{
				return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT;
			}
			break;
			case re::Texture::Format::RG16F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R16G16_FLOAT;
			}
			break;
			case re::Texture::Format::R16F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R16_FLOAT;
			}
			break;
			case re::Texture::Format::RGBA8: // 8 bits per channel x N channels
			{
				return texParams.m_colorSpace == re::Texture::ColorSpace::sRGB ?
					DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
			}
			break;
			case re::Texture::Format::RG8:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R8G8_UNORM;
			}
			break;
			case re::Texture::Format::R8:
			{
				return DXGI_FORMAT::DXGI_FORMAT_R8_UNORM;
			}
			break;
			case re::Texture::Format::Depth32F:
			{
				return DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT;
			}
			break;
			case re::Texture::Format::RGB8:
			{
				LOG_WARNING("Unsupported RGB8 Texture format requested, selecting R11G11B10 instead");
				return DXGI_FORMAT::DXGI_FORMAT_R11G11B10_FLOAT; // No matching DXGI_FORMAT, choose something ~similar
			}
			break;
			case re::Texture::Format::RGB16F: // No matching DXGI_FORMAT
			case re::Texture::Format::Invalid:
			default:
			{
				SEAssertF("Invalid format");
			}
		}
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	}


	Texture::PlatformParams::PlatformParams(re::Texture::TextureParams const& texParams)
	{
		m_format = GetTextureFormat(texParams);
	}


	Texture::PlatformParams::~PlatformParams()
	{
		m_format = DXGI_FORMAT_UNKNOWN;
		m_textureResource = nullptr;
		m_descriptor.Free(0);
	}


	void Texture::Create(re::Texture& texture)
	{
		dx12::Texture::PlatformParams* texPlatParams = texture.GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
		SEAssert("Texture is already created", texPlatParams->m_isCreated == false);

		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		re::Texture::TextureParams const& texParams = texture.GetTextureParams();

		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;

		switch (texParams.m_usage)
		{
		case re::Texture::Usage::Color:
		{
			LOG_ERROR("TODO: Support color textures: %s is not actually created", texture.GetName().c_str());
		}
		break;
		case re::Texture::Usage::ColorTarget:
		{
			LOG_ERROR("dx12::Texture::Create: Texture is marked as a target, doing nothing...");
		}
		break;
		case re::Texture::Usage::SwapchainColorProxy:
		{
			initialState = D3D12_RESOURCE_STATE_COMMON;
		}
		break;
		case re::Texture::Usage::DepthTarget:
		{
			re::Texture::TextureParams const& texParams = texture.GetTextureParams();

			// Note: We get/cache this value in the target during dx12::TextureTargetSet::CreateDepthStencilTarget, 
			// but re::Texture::TextureParams are const so it should be impossible to get a different result 
			const DXGI_FORMAT depthFormat = dx12::Texture::GetTextureFormat(texParams);
			// TODO: SHould the format be a member of the texture?

			// Clear values:
			D3D12_CLEAR_VALUE optimizedClearValue = {};
			optimizedClearValue.Format = depthFormat; 
			optimizedClearValue.DepthStencil = { 1.0f, 0 }; // Float depth, uint8_t stencil

			// Depth resource description:
			const int width = en::Config::Get()->GetValue<int>("windowXRes");
			const int height = en::Config::Get()->GetValue<int>("windowYRes");
			SEAssert("Invalid dimensions", width >= 1 && height >= 1);

			CD3DX12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				depthFormat,
				width,
				height,
				1,
				0,
				1,
				0,
				D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

			// Depth committed heap resources:
			dx12::Texture::PlatformParams* texPlatParams = 
				texture.GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

			CD3DX12_HEAP_PROPERTIES depthHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

			initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

			HRESULT hr = device->CreateCommittedResource(
				&depthHeapProperties,
				D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
				&depthResourceDesc,
				initialState,
				&optimizedClearValue,
				IID_PPV_ARGS(&texPlatParams->m_textureResource));
			texPlatParams->m_textureResource->SetName(texture.GetWName().c_str());
		}
		break;
		case re::Texture::Usage::Invalid:
		default:
		{
			SEAssertF("Invalid texture usage");
		}
		}

		texPlatParams->m_isCreated = true;
		texPlatParams->m_isDirty = true;

		// Register the resource with the global resource state tracker:
		dx12::Context::GetGlobalResourceStateTracker().RegisterResource(
			texPlatParams->m_textureResource.Get(),
			initialState);
	}


	// re::Texture::Create factory wrapper, for the DX12-specific case where we need to create a Texture resource using
	// an existing ID3D12Resource
	std::shared_ptr<re::Texture> Texture::CreateFromExistingResource(
		std::string const& name, 
		re::Texture::TextureParams const& params, 
		bool doClear, 
		Microsoft::WRL::ComPtr<ID3D12Resource> textureResource)
	{
		SEAssert("Invalid/unexpected texture format. For now, this function is used to create a backbuffer color target",
			params.m_usage == re::Texture::Usage::SwapchainColorProxy);

		// Note: re::Texture::Create will enroll the texture in API object creation, and eventually call the standard 
		// dx12::Texture::Create above
		std::shared_ptr<re::Texture> newTexture = re::Texture::Create(name, params, doClear);

		dx12::Texture::PlatformParams* texPlatParams = 
			newTexture->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
		SEAssert("Texture is already created", 
			!texPlatParams->m_isCreated && texPlatParams->m_textureResource == nullptr);

		texPlatParams->m_textureResource = textureResource;

		return newTexture;
	}


	void Texture::Destroy(re::Texture& texture)
	{
		dx12::Texture::PlatformParams* texPlatParams = texture.GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		// Unregister the resource from the global resource state tracker. Note: The resource might be null if it was
		// never created (e.g. a duplicate was detected after loading)
		if (texPlatParams->m_textureResource)
		{
			dx12::Context::GetGlobalResourceStateTracker().UnregisterResource(texPlatParams->m_textureResource.Get());
		}

		texPlatParams->m_textureResource = nullptr;
		texPlatParams->m_descriptor.Free(0);

		texPlatParams->m_isCreated = false;
		texPlatParams->m_isDirty = true;
	}
	
	
	void Texture::GenerateMipMaps(re::Texture& texture)
	{
		#pragma message("TODO: Implement dx12::Texture::GenerateMipMaps")
		SEAssertF("TODO: Implement this");
	}
}