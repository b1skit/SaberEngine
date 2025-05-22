// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Texture.h"

#include "Core/Interfaces/IPlatformObject.h"


namespace re
{
	class SwapChain
	{
	public:
		struct PlatObj : public core::IPlatObj
		{
			virtual ~PlatObj() = default;

			bool m_vsyncEnabled = false; // DX12: Disabled if tearing is enabled (ie. using a variable refresh display)
		};


	public:
		SwapChain();
		~SwapChain();

		void Create(re::Texture::Format);
		void Destroy();

		bool GetVSyncState() const;
		bool ToggleVSync() const; // Returns true if VSync is enabled, false otherwise

		re::SwapChain::PlatObj* GetPlatformObject() const { return m_platObj.get(); }
		void SetPlatformObject(std::unique_ptr<re::SwapChain::PlatObj> platObj) { m_platObj = std::move(platObj); }


	private:
		std::unique_ptr<re::SwapChain::PlatObj> m_platObj;
	};
}