// © 2024 Adam Badke. All rights reserved.
#include "Inventory.h"


namespace core
{
	void Inventory::Destroy()
	{
		{
			std::lock_guard<std::shared_mutex> lock(m_resourceSystemsMutex);

			for (auto& resourceSystem : m_resourceSystems)
			{
				resourceSystem.second->Destroy();
			}
			m_resourceSystems.clear();
		}
	}


	void Inventory::OnEndOfFrame()
	{
		{
			std::lock_guard<std::shared_mutex> lock(m_resourceSystemsMutex);

			for (auto& resourceSystem : m_resourceSystems)
			{
				resourceSystem.second->OnEndOfFrame();
			}
		}
	}
}