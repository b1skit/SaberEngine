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
		
		IDXGIAdapter4* GetD3DAdapter() const { return m_dxgiAdapter4.Get(); }
		ID3D12Device2* GetD3DDisplayDevice() const { return m_displayDevice.Get(); }

	private:
		Microsoft::WRL::ComPtr<IDXGIAdapter4> m_dxgiAdapter4 = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Device2> m_displayDevice = nullptr; // Display adapter device


	private:
		// Copying not allowed:
		Device(Device const&) = delete;
		Device& operator=(Device const&) = delete;
	};
}