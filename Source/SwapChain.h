// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace re
{
	class SwapChain
	{
	public:
		struct PlatformParams
		{
			PlatformParams() = default;

			// Copying not allowed
			PlatformParams(PlatformParams const&) = delete;
			PlatformParams(PlatformParams&&) = delete;
			PlatformParams& operator=(PlatformParams const&) = delete;
			virtual ~PlatformParams() = 0;
		};


	public:
		SwapChain();
		~SwapChain() { Destroy(); };

		void Create();
		void Destroy();

		void SetVSyncMode(bool enabled) const;

		re::SwapChain::PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<re::SwapChain::PlatformParams> params) { m_platformParams = std::move(params); }


	private:
		std::unique_ptr<re::SwapChain::PlatformParams> m_platformParams;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline SwapChain::PlatformParams::~PlatformParams() {};
}