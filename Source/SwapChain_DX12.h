// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "RenderManager_DX12.h"
#include "SwapChain.h"


namespace dx12
{
	class SwapChain
	{
	public:
		struct PlatformParams final : public virtual re::SwapChain::PlatformParams
		{
			Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain = nullptr;

			// Pointers to our backbuffer resources
			Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[dx12::RenderManager::k_numFrames];
			uint8_t m_backBufferIdx;

			DXGI_FORMAT m_displayFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

			bool m_vsyncEnabled = false; // Disabled if tearing is enabled (ie. using a variable refresh display)
			bool m_tearingSupported = false; // Always allow tearing if supported. Required for variable refresh dispays (eg. G-Sync/FreeSync)
		};


	public:
		static void Create(re::SwapChain& swapChain);
		static void Destroy(re::SwapChain& swapChain);
		static void SetVSyncMode(re::SwapChain const& swapChain, bool enabled);

		// DX12-specific functionality:
		static bool CheckTearingSupport(); // Variable refresh rate dispays (eg. G-Sync/FreeSync) require tearing enabled
	};
}