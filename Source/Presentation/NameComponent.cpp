// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "NameComponent.h"


namespace pr
{
	NameComponent& NameComponent::AttachNameComponent(EntityManager& em, entt::entity entity, char const* name)
	{
		return *em.EmplaceComponent<NameComponent>(entity, PrivateCTORTag{}, name);
	}
}