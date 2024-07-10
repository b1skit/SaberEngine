// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "CPUDescriptorHeapManager_DX12.h"
#include "DescriptorCache_DX12.h"
#include "Texture.h"

#include <d3d12.h>
#include <wrl.h>


namespace dx12
{
	class CommandList;


	class Texture
	{
	public:
		struct PlatformParams final : public re::Texture::PlatformParams
		{
			PlatformParams(re::Texture const&);

			DXGI_FORMAT m_format;
			Microsoft::WRL::ComPtr<ID3D12Resource> m_textureResource;

			mutable dx12::DescriptorCache m_srvDescriptors;
			mutable dx12::DescriptorCache m_uavDescriptors;
			
			mutable dx12::DescriptorCache m_rtvDescriptors;
			mutable dx12::DescriptorCache m_dsvDescriptors;
		};


	public:
		// DX12-specific functionality:
		static void Create(re::Texture&, 
			dx12::CommandList* copyCmdList,
			std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& intermediateResources);
		
		static std::shared_ptr<re::Texture> CreateFromExistingResource(
			std::string const& name, re::Texture::TextureParams const&, Microsoft::WRL::ComPtr<ID3D12Resource>);

		static D3D12_CPU_DESCRIPTOR_HANDLE GetSRV(re::Texture const*, re::TextureView const&);
		static D3D12_CPU_DESCRIPTOR_HANDLE GetUAV(re::Texture const*, re::TextureView const&);

		static D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(re::Texture const*, re::TextureView const&);
		static D3D12_CPU_DESCRIPTOR_HANDLE GetDSV(re::Texture const*, re::TextureView const&);

		static DXGI_FORMAT GetTextureFormat(re::Texture::TextureParams const&);
		static DXGI_FORMAT GetEquivalentUAVCompatibleFormat(DXGI_FORMAT format); // DXGI_FORMAT_UNKNOWN if none exists

		// Platform functionality:
		static void Destroy(re::Texture&);
	};
}