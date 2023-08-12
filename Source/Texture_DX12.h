// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>
#include <wrl.h>

#include "CPUDescriptorHeapManager_DX12.h"
#include "Texture.h"


namespace dx12
{
	class Texture
	{
	public:
		enum View : uint8_t
		{
			SRV,
			UAV,

			View_Count
		};

		struct PlatformParams final : public re::Texture::PlatformParams
		{
			PlatformParams(re::Texture::TextureParams const& texParams);

			~PlatformParams() override;

			DXGI_FORMAT m_format;
			Microsoft::WRL::ComPtr<ID3D12Resource> m_textureResource;

			// Each view type can have a view for each mip level
			std::array<std::vector<dx12::DescriptorAllocation>, View_Count> m_viewCpuDescAllocations;

			uint64_t m_modificationFence = 0; // Modified via a pointer when submitting command lists on a command queue
		};


	public:
		// DX12-specific functionality:
		static void Create(re::Texture&, 
			ID3D12GraphicsCommandList2*,
			std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& intermediateResources);
		
		static std::shared_ptr<re::Texture> CreateFromExistingResource(
			std::string const& name, re::Texture::TextureParams const&, bool doClear, Microsoft::WRL::ComPtr<ID3D12Resource>);

		static DXGI_FORMAT GetTextureFormat(re::Texture::TextureParams const&);


		// Platform functionality:
		static void Destroy(re::Texture&);
	};
}