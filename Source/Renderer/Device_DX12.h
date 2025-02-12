// © 2022 Adam Badke. All rights reserved.
#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "Fence_DX12.h"


namespace dx12
{
	class Device
	{
	public:
		Device();
		Device(Device&&) noexcept = default;
		Device& operator=(Device&&) noexcept = default;
		~Device() { Destroy(); };

		void Create();
		void Destroy();
		
		Microsoft::WRL::ComPtr<IDXGIAdapter> const& GetD3DAdapter() const { return m_dxgiAdapter; }
		Microsoft::WRL::ComPtr<ID3D12Device> const& GetD3DDevice() const { return m_device; }


	private:
		Microsoft::WRL::ComPtr<IDXGIAdapter> m_dxgiAdapter;
		Microsoft::WRL::ComPtr<ID3D12Device> m_device;


	private: // Copying not allowed:
		Device(Device const&) = delete;
		Device& operator=(Device const&) = delete;
	};
}