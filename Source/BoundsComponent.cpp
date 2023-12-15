// © 2023 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "GameplayManager.h"
#include "MarkerComponents.h"
#include "RenderDataComponent.h"


namespace fr
{
	void CreateSceneBoundsEntity(fr::GameplayManager& gpm)
	{
		constexpr char const* k_sceneBoundsName = "SceneBounds";

		entt::entity sceneBoundsEntity = gpm.CreateEntity(k_sceneBoundsName);

		gpm.EmplaceComponent<gr::Bounds>(sceneBoundsEntity);
		gpm.EmplaceComponent<DirtyMarker<gr::Bounds>>(sceneBoundsEntity);
		gpm.EmplaceComponent<gr::RenderDataComponent>(sceneBoundsEntity, 1);
		gpm.EmplaceComponent<IsSceneBoundsMarker>(sceneBoundsEntity);
	}


	void AttachBoundsComponent(fr::GameplayManager& gpm, entt::entity entity)
	{
		gpm.TryEmplaceComponent<gr::Bounds>(entity);
		gpm.EmplaceOrReplaceComponent<DirtyMarker<gr::Bounds>>(entity);
	}


	// ---


	BoundsComponent::BoundsComponent(glm::vec3 minXYZ, glm::vec3 maxXYZ)
	{

	}


	BoundsComponent::BoundsComponent(glm::vec3 minXYZ, glm::vec3 maxXYZ, std::vector<glm::vec3> const& positions)
	{

	}
}