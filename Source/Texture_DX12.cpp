// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Texture_DX12.h"


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
		#pragma message("TODO: Implement dx12::Texture::Create")
		LOG_ERROR("TODO: Implement dx12::Texture::Create");
	}


	void Texture::Destroy(re::Texture& texture)
	{
		#pragma message("TODO: Implement dx12::Texture::Destroy")
		LOG_ERROR("TODO: Implement dx12::Texture::Destroy");
	}
	
	
	void Texture::GenerateMipMaps(re::Texture& texture)
	{
		#pragma message("TODO: Implement dx12::Texture::GenerateMipMaps")
		SEAssertF("TODO: Implement this");
	}
}