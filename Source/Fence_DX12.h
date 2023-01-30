// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace dx12
{
	class Fence
	{
	public:
		Fence() = default;
		~Fence() = default;

		void Create();
		void Destroy();


	private:


	private:
		// Copying not allowed:
		Fence(Fence const&) = delete;
		Fence(Fence&&) = delete;
		Fence& operator=(Fence const&) = delete;
	};
}