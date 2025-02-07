// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "GPUTimer.h"

#include <wrl.h>
#include <d3d12.h>


namespace dx12
{
	class GPUTimer
	{
	public:
		struct PlatformParams final : public re::GPUTimer::PlatformParams
		{
			Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_gpuQueryHeap;
			Microsoft::WRL::ComPtr<ID3D12Resource> m_gpuQueryBuffer;

			uint64_t m_totalQueryBytesPerFrame = 0;			
		};


		struct CreateParams
		{
			ID3D12Device2* m_device = nullptr;
			ID3D12CommandQueue* m_directCommandQueue = nullptr;
		};


	public:
		static void Create(re::GPUTimer const&, void const* createParams); // dx12::GPUTimer::CreateParams const*
		static void Destroy(re::GPUTimer const&);

		static void BeginFrame(re::GPUTimer const&);
		static std::vector<uint64_t> EndFrame(re::GPUTimer const&, void*);

		static void StartTimer(re::GPUTimer const&, uint32_t startQueryIdx, void*);
		static void StopTimer(re::GPUTimer const&, uint32_t endQueryIdx, void*);
	};
}