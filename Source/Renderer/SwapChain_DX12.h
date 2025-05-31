// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "SwapChain.h"
#include "Texture.h"

#include <dxgi1_6.h>

#include <wrl/client.h>


namespace re
{
	class TextureTargetSet;
}

namespace dx12
{
	class SwapChain
	{
	public:
		struct PlatObj final : public re::SwapChain::PlatObj
		{
			Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain = nullptr;

			std::vector<std::shared_ptr<re::TextureTargetSet>> m_backbufferTargetSets;
			uint8_t m_backBufferIdx = std::numeric_limits<uint8_t>::max(); // Which backbuffer target set to use

			bool m_tearingSupported = false; // Always allow tearing if supported. Required for variable refresh dispays (eg. G-Sync/FreeSync)
		};


	public:
		static void Create(re::SwapChain&, re::Texture::Format);
		static void Destroy(re::SwapChain&);
		static bool ToggleVSync(re::SwapChain const&);

		static std::shared_ptr<re::TextureTargetSet> GetBackBufferTargetSet(re::SwapChain const&);
		static re::Texture::Format GetBackbufferFormat(re::SwapChain const&);
		static glm::uvec2 GetBackbufferDimensions(re::SwapChain const&);

	public: // DX12-specific functionality:
		static uint8_t GetCurrentBackBufferIdx(re::SwapChain const&);
		static uint8_t IncrementBackBufferIdx(re::SwapChain&); // Returns new backbuffer idx
	};
}