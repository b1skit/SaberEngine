// © 2023 Adam Badke. All rights reserved.
#include "Private/EntityManager.h"
#include "Private/NameComponent.h"


namespace fr
{
	NameComponent& NameComponent::AttachNameComponent(EntityManager& em, entt::entity entity, char const* name)
	{
		return *em.EmplaceComponent<NameComponent>(entity, PrivateCTORTag{}, name);
	}
}