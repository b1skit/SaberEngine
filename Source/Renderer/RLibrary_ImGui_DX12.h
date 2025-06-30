// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "RLibrary_ImGui_Platform.h"

#include "Core/Util/ThreadProtector.h"


struct ImGui_ImplDX12_InitInfo;

namespace re
{
	class Stage;
}

namespace dx12
{
	class RLibraryImGui final : public virtual platform::RLibraryImGui
	{
	public:
		struct PlatObj : public platform::RLibraryImGui::PlatObj
		{
			// ImGui callbacks:
			static void Allocate(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE*, D3D12_GPU_DESCRIPTOR_HANDLE*);
			static void Free(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE);


		protected:
			friend class RLibraryImGui;

			void InitializeImGuiSRVHeap();
			void DestroyImGuiSRVHeap();

			void CopyTempDescriptorToImGuiHeap( // Valid for a single frame only
				D3D12_CPU_DESCRIPTOR_HANDLE src,
				D3D12_CPU_DESCRIPTOR_HANDLE& cpuDstOut,
				D3D12_GPU_DESCRIPTOR_HANDLE& gpuDstOut);

			void FreeTempDescriptors(uint64_t currentFrame);
			
			struct TempDescriptorAllocation
			{
				D3D12_CPU_DESCRIPTOR_HANDLE m_cpuDesc;
				D3D12_GPU_DESCRIPTOR_HANDLE m_gpuDesc;
			};
			std::queue<std::pair<uint64_t, TempDescriptorAllocation>> m_deferredDescriptorDelete; // <Frame num, alloc info>


		private:
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_imGuiGPUVisibleSRVDescriptorHeap;

			D3D12_CPU_DESCRIPTOR_HANDLE m_heapStartCPU;
			D3D12_GPU_DESCRIPTOR_HANDLE m_heapStartGPU;
			uint32_t m_handleIncrementSize;
			std::vector<uint32_t> m_freeIndices;

			static constexpr D3D12_DESCRIPTOR_HEAP_TYPE k_heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			static constexpr uint32_t k_imguiHeapSize = 128;

			// The ImGui descriptor heap is not thread-safe; We use a thread protector to ensure we don't forget
			util::ThreadProtector m_threadProtector = util::ThreadProtector(false);
		};


	public:
		static std::unique_ptr<platform::RLibrary> Create();

		
		static void CopyTempDescriptorToImGuiHeap( // Valid for a single frame only
			D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor,
			D3D12_CPU_DESCRIPTOR_HANDLE& cpuDstOut,
			D3D12_GPU_DESCRIPTOR_HANDLE& gpuDstOut);


	public:
		RLibraryImGui() = default;
		~RLibraryImGui() = default;

		void Execute(gr::Stage*, void* platformObject) override;

		void Destroy() override;


	private:

	};
}