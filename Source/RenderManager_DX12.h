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
		static uint8_t GetNumFramesInFlight(); // Number of frames in flight


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
		// TODO: We should allow N frames of intermediate resources to be held. This is a potential (albiet unlikely)
		// bottleneck that will prevent the CPU from ever getting more than a single frame ahead
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_intermediateResources; // From resources created in the previous frame
		uint64_t m_intermediateResourceFenceVal;
	};


	inline uint8_t RenderManager::GetNumFramesInFlight()
	{
		static const uint8_t k_numFrames = dynamic_cast<dx12::RenderManager*>(re::RenderManager::Get())->k_numFrames;
		return k_numFrames;
	}
}