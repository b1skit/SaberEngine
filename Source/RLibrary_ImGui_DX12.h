// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "RLibrary_ImGui_Platform.h"

#include <wrl.h>
#include <d3d12.h>


namespace re
{
	class RenderStage;
}

namespace dx12
{
	class RLibraryImGui final : public virtual platform::RLibraryImGui
	{
	public:
		struct PlatformParams : public platform::RLibraryImGui::PlatformParams
		{
			// Imgui descriptor heap: A single, CPU and GPU-visible SRV descriptor for the internal font texture
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_imGuiGPUVisibleSRVDescriptorHeap;
		};


	public:
		static std::unique_ptr<platform::RLibrary> Create();

	public:
		RLibraryImGui() = default;
		~RLibraryImGui() = default;

		void Execute(re::RenderStage*) override;

		void Destroy() override;


	private:

	};
}