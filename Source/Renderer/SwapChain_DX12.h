// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "CPUDescriptorHeapManager_DX12.h"
#include "RenderManager_DX12.h"
#include "SwapChain.h"
#include "TextureTarget.h"

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>


namespace dx12
{
	class SwapChain
	{
	public:
		struct PlatformParams final : public re::SwapChain::PlatformParams
		{
			Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain = nullptr;

			std::vector<std::shared_ptr<re::TextureTargetSet>> m_backbufferTargetSets;
			uint8_t m_backBufferIdx = std::numeric_limits<uint8_t>::max(); // Which backbuffer target set to use

			bool m_tearingSupported = false; // Always allow tearing if supported. Required for variable refresh dispays (eg. G-Sync/FreeSync)
		};


	public:
		static void Create(re::SwapChain& swapChain);
		static void Destroy(re::SwapChain& swapChain);
		static bool ToggleVSync(re::SwapChain const& swapChain);

		// DX12-specific functionality:
		static uint8_t GetCurrentBackBufferIdx(re::SwapChain const& swapChain);
		static uint8_t IncrementBackBufferIdx(re::SwapChain& swapChain); // Returns new backbuffer idx
		static std::shared_ptr<re::TextureTargetSet> GetBackBufferTargetSet(re::SwapChain const& swapChain);
	};
}