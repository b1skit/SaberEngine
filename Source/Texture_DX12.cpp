// © 2022 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Texture_DX12.h"


namespace dx12
{
	Texture::PlatformParams::PlatformParams(re::Texture::TextureParams const& texParams)
	{
		#pragma message("TODO: Implement dx12::PlatformParams::PlatformParams")
		LOG_ERROR("TODO: Implement dx12::PlatformParams::PlatformParams");
	}


	Texture::PlatformParams::~PlatformParams()
	{
		#pragma message("TODO: Implement dx12::PlatformParams::~PlatformParams")
		LOG_ERROR("TODO: Implement dx12::PlatformParams::PlatformParams");
	}


	void Texture::Create(re::Texture& texture)
	{
		#pragma message("TODO: Implement dx12::PlatformParams::Create")
		SEAssertF("TODO: Implement this");
	}


	void Texture::Destroy(re::Texture& texture)
	{
		#pragma message("TODO: Implement dx12::PlatformParams::Destroy")
		SEAssertF("TODO: Implement this");
	}


	void Texture::Bind(re::Texture& texture, uint32_t textureUnit)
	{
		#pragma message("TODO: Implement dx12::PlatformParams::Bind")
		SEAssertF("TODO: Implement this");
	}
	
	
	void Texture::GenerateMipMaps(re::Texture& texture)
	{
		#pragma message("TODO: Implement dx12::PlatformParams::GenerateMipMaps")
		SEAssertF("TODO: Implement this");
	}
}