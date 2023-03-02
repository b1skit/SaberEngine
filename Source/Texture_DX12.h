// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>
#include <wrl.h>

#include "Texture.h"

struct CD3DX12_CPU_DESCRIPTOR_HANDLE;


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

			Microsoft::WRL::ComPtr<ID3D12Resource> m_textureResource;
		};


	public:
		static void Create(re::Texture&);
		static void CreateFromExistingResource(
			re::Texture&, Microsoft::WRL::ComPtr<ID3D12Resource>, CD3DX12_CPU_DESCRIPTOR_HANDLE const&); // For creating backbuffer target textures
		static void Destroy(re::Texture&);
		static void GenerateMipMaps(re::Texture&);
	};
}