// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "GPUTimer.h"


namespace platform
{
	class GPUTimer;
}

namespace dx12
{
	class GPUTimer
	{
	public:
		struct PlatObj final : public re::GPUTimer::PlatObj
		{
			void Destroy() override;

			Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_directComputeQueryHeap;
			Microsoft::WRL::ComPtr<ID3D12Resource> m_directComputeQueryBuffer;

			Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_copyQueryHeap;
			Microsoft::WRL::ComPtr<ID3D12Resource> m_copyQueryBuffer;
			bool m_copyQueriesSupported = false;

			uint64_t m_totalQueryBytesPerFrame = 0;			
		};


	public:
		static void Create(re::GPUTimer const&);
		// Destroy is handled via GPUTimer::PlatObj

		static void BeginFrame(re::GPUTimer const&);
		static std::vector<uint64_t> EndFrame(re::GPUTimer const&, re::GPUTimer::TimerType);

		static void StartTimer(re::GPUTimer const&, re::GPUTimer::TimerType, uint32_t startQueryIdx, void*);
		static void StopTimer(re::GPUTimer const&, re::GPUTimer::TimerType, uint32_t endQueryIdx, void*);
	};
}