// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "AccessKey.h"
#include "InvPtr.h"
#include "ResourceSystem.h"


namespace re
{
	class Context;
}
namespace core
{
	class Inventory final
	{
	public:
		using AccessKey = accesscontrol::AccessKey<re::Context>;
		static void OnEndOfFrame(AccessKey);
		static void Destroy(AccessKey);


	public: // All resource requests come through here:
		template<typename T>
		static core::InvPtr<T> Get(
			util::HashKey, // ID for the T
			std::shared_ptr<core::ILoadContext<T>> = nullptr); // Can only be null if the resource already exists

		template<typename T>
		static bool HasLoaded(util::HashKey); // Has the Resource been requested, and finished loading?

		template<typename T>
		static bool Has(util::HashKey); // Has the Resource been requested?


	private:
		template<typename T>
		static ResourceSystem<T>* GetCreateResourceSystem();


	private:
		static std::unordered_map<std::type_index, std::unique_ptr<IResourceSystem>> s_resourceSystems;
		static std::shared_mutex s_resourceSystemsMutex;


	private:
		Inventory() = delete;
		Inventory(Inventory const&) = delete;
		Inventory(Inventory&&) noexcept = delete;
		Inventory& operator=(Inventory const&) = delete;
		Inventory& operator=(Inventory&&) noexcept = delete;
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
	bool Inventory::HasLoaded(util::HashKey ID)
	{
		const std::type_index typeIdx = std::type_index(typeid(T));

		ResourceSystem<T> const* resourceSystem = nullptr;

		{
			std::shared_lock readLock(s_resourceSystemsMutex);

			auto iResourceSystemItr = s_resourceSystems.find(typeIdx);
			if (iResourceSystemItr != s_resourceSystems.end())
			{
				resourceSystem = dynamic_cast<ResourceSystem<T> const*>(iResourceSystemItr->second.get());
			}
		}

		return resourceSystem != nullptr && resourceSystem->HasLoaded(ID);
	}


	template<typename T>
	bool Inventory::Has(util::HashKey ID)
	{
		const std::type_index typeIdx = std::type_index(typeid(T));

		ResourceSystem<T> const* resourceSystem = nullptr;

		{
			std::shared_lock readLock(s_resourceSystemsMutex);

			auto iResourceSystemItr = s_resourceSystems.find(typeIdx);
			if (iResourceSystemItr != s_resourceSystems.end())
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
			std::shared_lock readLock(s_resourceSystemsMutex);

			auto iResourceSystemItr = s_resourceSystems.find(typeIdx);
			if (iResourceSystemItr == s_resourceSystems.end())
			{
				readLock.unlock();

				// Convert to a write lock, and create the ResourceSystem:
				{
					std::unique_lock<std::shared_mutex> writeLock(s_resourceSystemsMutex);

					iResourceSystemItr = s_resourceSystems.find(typeIdx);
					if (iResourceSystemItr == s_resourceSystems.end())
					{
						iResourceSystemItr = s_resourceSystems.emplace(
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