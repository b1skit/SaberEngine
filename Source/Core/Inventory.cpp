// © 2024 Adam Badke. All rights reserved.
#include "Inventory.h"

#include "ProfilingMarkers.h"


namespace core
{
	std::unordered_map<std::type_index, std::unique_ptr<IResourceSystem>> Inventory::s_resourceSystems;
	std::shared_mutex Inventory::s_resourceSystemsMutex;


	void Inventory::Destroy(AccessKey)
	{
		{
			std::lock_guard<std::shared_mutex> lock(s_resourceSystemsMutex);

			for (auto& resourceSystem : s_resourceSystems)
			{
				resourceSystem.second->Destroy();
			}
			s_resourceSystems.clear();
		}
	}


	void Inventory::OnEndOfFrame(AccessKey)
	{
		SEBeginCPUEvent("Inventory::OnEndOfFrame");

		{
			std::lock_guard<std::shared_mutex> lock(s_resourceSystemsMutex);

			for (auto& resourceSystem : s_resourceSystems)
			{
				resourceSystem.second->OnEndOfFrame();
			}
		}

		SEEndCPUEvent(); // "Inventory::OnEndOfFrame"
	}
}