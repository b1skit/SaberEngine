// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "BindlessResourceManager.h"


namespace platform
{
	struct IBindlessResource
	{
		static void (*GetResourceUseState)(void* dest, size_t destByteSize);
	};


	// ---


	class BindlessResourceManager
	{
	public:
		static std::unique_ptr<re::BindlessResourceManager::PlatObj> CreatePlatformObject();


	public:
		static void (*Initialize)(re::BindlessResourceManager&, uint8_t numFramesInFlight, uint64_t frameNum);
		static void (*SetResource)(re::BindlessResourceManager&, re::IBindlessResource*, ResourceHandle);
	};
}