// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RenderManager.h"

#include <wrl.h>
#include <d3d12.h>


enum D3D_FEATURE_LEVEL;

namespace dx12
{
	class RenderManager : public virtual re::RenderManager
	{
	public:
		RenderManager();
		~RenderManager() override = default;

	public:
		static uint8_t GetNumFrames(); // Number of frames in flight


	public: // Platform PIMPL:
		static void Initialize(re::RenderManager&);
		static void Shutdown(re::RenderManager&);
		static void CreateAPIResources(re::RenderManager&);

		static void StartImGuiFrame();
		static void RenderImGui();


	private: // re::RenderManager interface:
		void Render() override;
		

	protected:
		const uint8_t k_numFrames;


	private:

		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_intermediateResources; // From resources created in the previous frame
		uint64_t m_intermediateResourceFenceVal;
	};


	inline uint8_t RenderManager::GetNumFrames()
	{
		static const uint8_t k_numFrames = dynamic_cast<dx12::RenderManager*>(re::RenderManager::Get())->k_numFrames;
		return k_numFrames;
	}
}