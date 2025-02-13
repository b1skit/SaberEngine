#pragma once
#include "AccelerationStructureManager.h"


namespace dx12
{
	class AccelerationStructureManager
	{
	public:
		struct PlatformParams final : public virtual re::AccelerationStructureManager::PlatformParams
		{
			//
		};


	public:
		static void Create(re::AccelerationStructureManager&);
		static void Update(re::AccelerationStructureManager&);
		static void Destroy(re::AccelerationStructureManager&);
	};
}