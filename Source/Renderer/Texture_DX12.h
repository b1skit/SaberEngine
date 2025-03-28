// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "DescriptorCache_DX12.h"
#include "Texture.h"

#include <d3d12.h>
#include <wrl.h>


namespace dx12
{
	class CommandList;
	class GPUResource;


	class Texture
	{
	public:
		struct PlatformParams final : public re::Texture::PlatformParams
		{
			PlatformParams(re::Texture&);
			~PlatformParams();

			void Destroy() override;

			std::unique_ptr<dx12::GPUResource> m_gpuResource;

			DXGI_FORMAT m_format;

			mutable dx12::DescriptorCache m_srvDescriptors;
			mutable dx12::DescriptorCache m_uavDescriptors;
			
			mutable dx12::DescriptorCache m_rtvDescriptors;
			mutable dx12::DescriptorCache m_dsvDescriptors;
		};


	public:
		// DX12-specific functionality:
		static void Create(core::InvPtr<re::Texture> const&, void* dx12CopyCmdList);
		
		static core::InvPtr<re::Texture> CreateFromExistingResource(
			std::string const& name, re::Texture::TextureParams const&, Microsoft::WRL::ComPtr<ID3D12Resource>);

		static D3D12_CPU_DESCRIPTOR_HANDLE GetSRV(core::InvPtr<re::Texture> const&, re::TextureView const&);
		static D3D12_CPU_DESCRIPTOR_HANDLE GetUAV(core::InvPtr<re::Texture> const&, re::TextureView const&);

		static D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(core::InvPtr<re::Texture> const&, re::TextureView const&);
		static D3D12_CPU_DESCRIPTOR_HANDLE GetDSV(core::InvPtr<re::Texture> const&, re::TextureView const&);

		static DXGI_FORMAT GetTextureFormat(re::Texture::TextureParams const&);
		static DXGI_FORMAT GetEquivalentUAVCompatibleFormat(DXGI_FORMAT format); // DXGI_FORMAT_UNKNOWN if none exists

		// Platform functionality:
		static void Destroy(re::Texture&);
		static void ShowImGuiWindow(core::InvPtr<re::Texture> const&, float scale);
	};
}