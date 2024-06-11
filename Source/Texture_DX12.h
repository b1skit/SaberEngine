// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>
#include <wrl.h>

#include "CPUDescriptorHeapManager_DX12.h"
#include "Texture.h"


namespace dx12
{
	class CommandList;


	class Texture
	{
	public:
		struct PlatformParams final : public re::Texture::PlatformParams
		{
			PlatformParams(re::Texture const&);

			~PlatformParams() override;

			DXGI_FORMAT m_format;
			Microsoft::WRL::ComPtr<ID3D12Resource> m_textureResource;

			dx12::DescriptorAllocation m_srvCpuDescAllocations; // Indexed by texture dimension (1 per dimension)
			dx12::DescriptorAllocation m_uavCpuDescAllocations; // Indexed by array/face/mip
		};


	public:
		// DX12-specific functionality:
		static void Create(re::Texture&, 
			dx12::CommandList* copyCmdList,
			std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& intermediateResources);
		
		static std::shared_ptr<re::Texture> CreateFromExistingResource(
			std::string const& name, re::Texture::TextureParams const&, Microsoft::WRL::ComPtr<ID3D12Resource>);

		static DXGI_FORMAT GetTextureFormat(re::Texture::TextureParams const&);


		// Platform functionality:
		static void Destroy(re::Texture&);
	};
}