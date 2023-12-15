// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Bounds.h"
#include "RenderDataIDs.h"



namespace fr
{
	class GameplayManager;


	struct IsSceneBoundsMarker {}; //Unique: Only added to 1 bounds component for the entire scene

	void CreateSceneBoundsEntity(fr::GameplayManager&);

	void AttachBoundsComponent(fr::GameplayManager&, entt::entity);


	class BoundsComponent
	{
	public:
		BoundsComponent() = default;
		BoundsComponent(glm::vec3 minXYZ, glm::vec3 maxXYZ);
		BoundsComponent(glm::vec3 minXYZ, glm::vec3 maxXYZ, std::vector<glm::vec3> const& positions);

		gr::Bounds& GetMainBounds() { return m_bounds; }

		std::vector<gr::Bounds>& GetPrimitiveBounds() { return m_primitiveBounds; }

	private:
		gr::Bounds m_bounds; // Main bounds
		std::vector<gr::Bounds> m_primitiveBounds;
	};

	struct BoundsRenderData
	{
		glm::vec3 m_minXYZ;
		glm::vec3 m_maxXYZ;
	};


	struct BoundsCollectionRenderData
	{
		const BoundsRenderData m_bounds; // Main Bounds: Contains all primitive bounds

		std::vector<BoundsRenderData> m_primitiveBounds; // Bounds of any primitives contained within the main bounds
	};
}