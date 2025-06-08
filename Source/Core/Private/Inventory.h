// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Private/InvPtr.h"
#include "Private/ResourceSystem.h"


namespace core
{
	class Inventory final
	{
	public:
		Inventory() = default;

		Inventory(Inventory const&) = default;
		Inventory(Inventory&&) = default;

		Inventory& operator=(Inventory const&) = default;
		Inventory& operator=(Inventory&&) = default;

		~Inventory() = default;

		void Destroy();


	public:
		void OnEndOfFrame();


	public: // All resource requests come through here:
		template<typename T>
		core::InvPtr<T> Get(
			util::HashKey, // ID for the T
			std::shared_ptr<core::ILoadContext<T>> = nullptr); // Can only be null if the resource already exists

		template<typename T>
		bool HasLoaded(util::HashKey) const; // Has the Resource been requested, and finished loading?

		template<typename T>
		bool Has(util::HashKey) const; // Has the Resource been requested?


	private:
		template<typename T>
		ResourceSystem<T>* GetCreateResourceSystem();


	private:
		std::unordered_map<std::type_index, std::unique_ptr<IResourceSystem>> m_resourceSystems;
		mutable std::shared_mutex m_resourceSystemsMutex;
	};


	template<typename T>
	core::InvPtr<T> Inventory::Get(
		util::HashKey ID, std::shared_ptr<core::ILoadContext<T>> loadContext /*= nullptr*/)
	{
		ResourceSystem<T>* resourceSystem = GetCreateResourceSystem<T>();

		auto controlBlock = resourceSystem->Get<T>(ID, loadContext);

		return core::InvPtr<T>::Create(controlBlock);
	}


	template<typename T>
	bool Inventory::HasLoaded(util::HashKey ID) const
	{
		const std::type_index typeIdx = std::type_index(typeid(T));

		ResourceSystem<T> const* resourceSystem = nullptr;

		{
			std::shared_lock readLock(m_resourceSystemsMutex);

			auto iResourceSystemItr = m_resourceSystems.find(typeIdx);
			if (iResourceSystemItr != m_resourceSystems.end())
			{
				resourceSystem = dynamic_cast<ResourceSystem<T> const*>(iResourceSystemItr->second.get());
			}
		}

		return resourceSystem != nullptr && resourceSystem->HasLoaded(ID);
	}


	template<typename T>
	bool Inventory::Has(util::HashKey ID) const
	{
		const std::type_index typeIdx = std::type_index(typeid(T));

		ResourceSystem<T> const* resourceSystem = nullptr;

		{
			std::shared_lock readLock(m_resourceSystemsMutex);

			auto iResourceSystemItr = m_resourceSystems.find(typeIdx);
			if (iResourceSystemItr != m_resourceSystems.end())
			{
				resourceSystem = dynamic_cast<ResourceSystem<T> const*>(iResourceSystemItr->second.get());
			}
		}

		return resourceSystem != nullptr && resourceSystem->Has(ID);
	}


	template<typename T>
	ResourceSystem<T>* Inventory::GetCreateResourceSystem()
	{
		ResourceSystem<T>* resourceSystem = nullptr;

		const std::type_index typeIdx = std::type_index(typeid(T));
		{
			std::shared_lock readLock(m_resourceSystemsMutex);

			auto iResourceSystemItr = m_resourceSystems.find(typeIdx);
			if (iResourceSystemItr == m_resourceSystems.end())
			{
				readLock.unlock();

				// Convert to a write lock, and create the ResourceSystem:
				{
					std::unique_lock<std::shared_mutex> writeLock(m_resourceSystemsMutex);

					iResourceSystemItr = m_resourceSystems.find(typeIdx);
					if (iResourceSystemItr == m_resourceSystems.end())
					{
						iResourceSystemItr = m_resourceSystems.emplace(
							typeIdx,
							new ResourceSystem<T>()).first;

						resourceSystem = dynamic_cast<ResourceSystem<T>*>(iResourceSystemItr->second.get());
					}
					else // The ResourceSystem was created while we waited, just get it
					{
						resourceSystem = dynamic_cast<ResourceSystem<T>*>(iResourceSystemItr->second.get());
					}
				}
			}
			else // Otherwise, we still hold the read lock here so get the pointer:
			{
				resourceSystem = dynamic_cast<ResourceSystem<T>*>(iResourceSystemItr->second.get());
			}
		}
		SEAssert(resourceSystem, "Failed to find or create a ResourceSystem");

		return resourceSystem;
	}
}