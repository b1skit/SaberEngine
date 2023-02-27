// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>

#include "Texture.h"


namespace dx12
{
	class Texture
	{
	public:
		static DXGI_FORMAT GetTextureFormat(re::Texture::TextureParams const&);


	public:
		struct PlatformParams final : public virtual re::Texture::PlatformParams
		{
			PlatformParams(re::Texture::TextureParams const& texParams);

			~PlatformParams() override;
		};


	public:
		static void Create(re::Texture& texture);
		static void Destroy(re::Texture& texture);
		static void GenerateMipMaps(re::Texture& texture);
	};
}