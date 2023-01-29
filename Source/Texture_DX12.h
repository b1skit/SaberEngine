// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Texture.h"


namespace dx12
{
	class Texture
	{
	public:
		struct PlatformParams final : public virtual re::Texture::PlatformParams
		{
			PlatformParams(re::Texture::TextureParams const& texParams);

			~PlatformParams() override;
		};


	public:
		static void Create(re::Texture& texture);
		static void Bind(re::Texture& texture, uint32_t textureUnit);
		static void Destroy(re::Texture& texture);
		static void GenerateMipMaps(re::Texture& texture);
	};
}