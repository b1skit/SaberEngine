// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "BindlessResourceManager.h"


namespace platform
{
	class IBindlessResourceSet
	{
	public:
		static std::unique_ptr<re::IBindlessResourceSet::PlatformParams> CreatePlatformParams();


	public:
		static void (*Initialize)(re::IBindlessResourceSet&);
		static void (*SetResource)(re::IBindlessResourceSet&, re::IBindlessResource*, ResourceHandle);
	};


	// ---


	class BindlessResourceManager
	{
	public:
		static std::unique_ptr<re::BindlessResourceManager::PlatformParams> CreatePlatformParams(re::BindlessResourceManager&);


	public:
		static void (*Create)(re::BindlessResourceManager&, uint32_t totalDescriptors);
	};
}