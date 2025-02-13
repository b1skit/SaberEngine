// © 2025 Adam Badke. All rights reserved.
#include "AccelerationStructureManager.h"
#include "AccelerationStructureManager_Platform.h"
#include "SysInfo_Platform.h"

#include "Core/Assert.h"
#include "Core/Logger.h"


namespace re
{
	void AccelerationStructureManager::Create()
	{
		SEAssert(platform::SysInfo::GetRayTracingSupport(),
			"Creating an AccelerationStructureManager, but the system does not support ray tracing. This is unexpected");

		LOG("Creating AccelerationStructureManager");
		
		platform::AccelerationStructureManager::CreatePlatformParams(*this);

		platform::AccelerationStructureManager::Create(*this);
	}


	void AccelerationStructureManager::Update()
	{
		platform::AccelerationStructureManager::Update(*this);
	}


	void AccelerationStructureManager::Destroy()
	{
		LOG("Destroying AccelerationStructureManager");

		platform::AccelerationStructureManager::Destroy(*this);
	}
}