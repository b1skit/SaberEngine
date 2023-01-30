// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>
#include <d3d12.h>

#include "RenderManager_DX12.h"


namespace dx12
{
	class Fence
	{
	public:
		Fence();
		~Fence() = default;

		void Create();
		void Destroy();

		uint64_t Signal();
		void Wait();
		void Flush();

	private:


	private:
		// Copying not allowed:
		Fence(Fence const&) = delete;
		Fence(Fence&&) = delete;
		Fence& operator=(Fence const&) = delete;
	};
}