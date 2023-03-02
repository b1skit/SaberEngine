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
			case re::Texture::Format::RGB16F: // No matching DXGI_FORMAT
			case re::Texture::Format::RGB8: // No matching DXGI_FORMAT
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
		#pragma message("TODO: Implement dx12::Texture::PlatformParams::PlatformParams")
		LOG_ERROR("TODO: Implement dx12::Texture::PlatformParams::PlatformParams");
	}

	Texture::PlatformParams::~PlatformParams()
	{
		#pragma message("TODO: Implement dx12::Texture::PlatformParams::~PlatformParams")
		LOG_ERROR("TODO: Implement dx12::Texture::PlatformParams::PlatformParams");
	}


	void Texture::Create(re::Texture& texture)
	{
		dx12::Texture::PlatformParams* const texPlatParams =
			dynamic_cast<dx12::Texture::PlatformParams*>(texture.GetPlatformParams());
		if (texPlatParams->m_isCreated)
		{
			return;
		}
		texPlatParams->m_isCreated = true;

		re::Context const& context = re::RenderManager::Get()->GetContext();

		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(context.GetPlatformParams());

		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();


		re::Texture::TextureParams const& texParams = texture.GetTextureParams();

		switch (texParams.m_usage)
		{
		case re::Texture::Usage::Color:
		{
			SEAssertF("TODO: Support color textures!");
		}
		break;
		case re::Texture::Usage::ColorTarget:
		{
			SEAssertF("TODO: Support color textures!");
		}
		break;
		case re::Texture::Usage::DepthTarget:
		{
			const DXGI_FORMAT depthFormat = dx12::Texture::GetTextureFormat(texParams);

			const int width = en::Config::Get()->GetValue<int>("windowXRes");
			const int height = en::Config::Get()->GetValue<int>("windowYRes");
			SEAssert("Invalid dimensions", width >= 1 && height >= 1);

			D3D12_CLEAR_VALUE optimizedClearValue = {};
			optimizedClearValue.Format = depthFormat;
			optimizedClearValue.DepthStencil = { 1.0f, 0 }; // Float depth, uint8_t stencil

			CD3DX12_HEAP_PROPERTIES depthHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

			CD3DX12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				depthFormat,
				width,
				height,
				1,
				0,
				1,
				0,
				D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

			HRESULT hr = device->CreateCommittedResource(
				&depthHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&depthResourceDesc,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&optimizedClearValue,
				IID_PPV_ARGS(&texPlatParams->m_textureResource)
			);

			// Update the depth-stencil view
			D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
			dsv.Format = depthFormat;
			dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsv.Texture2D.MipSlice = 0;
			dsv.Flags = D3D12_DSV_FLAG_NONE;

			CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescHandle(ctxPlatParams->m_DSVHeap->GetCPUDescriptorHandleForHeapStart());

			device->CreateDepthStencilView(
				texPlatParams->m_textureResource.Get(),
				&dsv,
				dsvDescHandle);
		}
		break;
		case re::Texture::Usage::Invalid:
		default:
		{
			SEAssertF("Invalid texture usage");
		}
		}
	}


	// TODO: descHandle should be retrieved directly from a descriptor heap manager!!!!
	void Texture::CreateFromExistingResource(
		re::Texture& texture, ComPtr<ID3D12Resource> bufferResource, CD3DX12_CPU_DESCRIPTOR_HANDLE const& descHandle)
	{
		SEAssert("Buffer cannot be null", bufferResource);

		re::Context const& context = re::RenderManager::Get()->GetContext();
		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(context.GetPlatformParams());

		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		re::Texture::TextureParams const& texParams = texture.GetTextureParams();

		dx12::Texture::PlatformParams* const texPlatParams =
			dynamic_cast<dx12::Texture::PlatformParams*>(texture.GetPlatformParams());
		
		SEAssert("We only currently handle color target creation here (i.e. from the backbuffer resource)", 
			texParams.m_usage == re::Texture::Usage::ColorTarget);

		// Create the RTV:
		device->CreateRenderTargetView(
			bufferResource.Get(), // Pointer to the resource containing the render target texture
			nullptr,  // Pointer to a render target view descriptor. nullptr = default
			descHandle); // Descriptor destination

		texPlatParams->m_textureResource = bufferResource;
		texPlatParams->m_isCreated = true;
	}


	void Texture::Destroy(re::Texture& texture)
	{
		dx12::Texture::PlatformParams* const texPlatParams =
			dynamic_cast<dx12::Texture::PlatformParams*>(texture.GetPlatformParams());

		texPlatParams->m_textureResource = nullptr;
	}
	
	
	void Texture::GenerateMipMaps(re::Texture& texture)
	{
		#pragma message("TODO: Implement dx12::Texture::GenerateMipMaps")
		SEAssertF("TODO: Implement this");
	}
}