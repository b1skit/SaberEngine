// © 2023 Adam Badke. All rights reserved.
#include "GameplayManager.h"
#include "NameComponent.h"


namespace fr
{
	NameComponent& NameComponent::AttachNameComponent(GameplayManager& gpm, entt::entity entity, char const* name)
	{
		return *gpm.EmplaceComponent<NameComponent>(entity, PrivateCTORTag{}, name);
	}
}