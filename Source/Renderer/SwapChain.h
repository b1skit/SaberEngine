// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/IPlatformParams.h"


namespace re
{
	class SwapChain
	{
	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual ~PlatformParams() = default;

			bool m_vsyncEnabled = false; // DX12: Disabled if tearing is enabled (ie. using a variable refresh display)
		};


	public:
		SwapChain();
		~SwapChain();

		void Create();
		void Destroy();

		bool GetVSyncState() const;
		bool ToggleVSync() const; // Returns true if VSync is enabled, false otherwise

		re::SwapChain::PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<re::SwapChain::PlatformParams> params) { m_platformParams = std::move(params); }


	private:
		std::unique_ptr<re::SwapChain::PlatformParams> m_platformParams;
	};
}