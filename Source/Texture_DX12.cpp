// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Texture_DX12.h"


namespace dx12
{
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
		SEAssertF("TODO: Implement this");
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