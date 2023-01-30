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
		Device() = default;
		~Device() = default;

		void Create();
		void Destroy();
		
		Microsoft::WRL::ComPtr<IDXGIAdapter4> GetAdapter() { return m_dxgiAdapter4; }
		Microsoft::WRL::ComPtr<ID3D12Device2> GetDisplayDevice() { return m_displayDevice; }

	private:
		Microsoft::WRL::ComPtr<IDXGIAdapter4> m_dxgiAdapter4 = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Device2> m_displayDevice = nullptr; // Display adapter device

		dx12::Fence m_fence;


	private:
		// Copying not allowed:
		Device(Device const&) = delete;
		Device(Device&&) = delete;
		Device& operator=(Device const&) = delete;
	};
}