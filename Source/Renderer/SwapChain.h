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
		};


	public:
		SwapChain();
		~SwapChain();

		void Create();
		void Destroy();

		void SetVSyncMode(bool enabled) const;

		re::SwapChain::PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<re::SwapChain::PlatformParams> params) { m_platformParams = std::move(params); }


	private:
		std::unique_ptr<re::SwapChain::PlatformParams> m_platformParams;
	};
}