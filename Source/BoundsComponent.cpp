// © 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "GameplayManager.h"
#include "MarkerComponents.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "Transform.h"


using glm::vec3;
using glm::vec4;
using glm::mat4;
using fr::Transform;


namespace
{
	constexpr float k_bounds3DDepthBias = 0.01f; // Offset to ensure axis min != axis max
}

namespace fr
{
	void Bounds::CreateSceneBounds(fr::GameplayManager& gpm)
	{
		constexpr char const* k_sceneBoundsName = "SceneBounds";

		entt::entity sceneBoundsEntity = gpm.CreateEntity(k_sceneBoundsName);

		fr::TransformComponent& sceneBoundsTransformComponent = 
			fr::TransformComponent::AttachTransformComponent(gpm, sceneBoundsEntity, nullptr);
		
		gr::RenderDataComponent::AttachNewRenderDataComponent(
			gpm, sceneBoundsEntity, sceneBoundsTransformComponent.GetTransformID());

		// Attach the BoundsComponent now that the RenderDataComponent is attached:
		AttachBoundsComponent(gpm, sceneBoundsEntity);

		gpm.EmplaceComponent<IsSceneBoundsMarker>(sceneBoundsEntity);
	}


	void Bounds::AttachBoundsComponent(fr::GameplayManager& gpm, entt::entity entity)
	{
		SEAssert("Bounds can only be attached to an entity that already has a RenderDataComponent",
			fr::Relationship::HasComponentInParentHierarchy<gr::RenderDataComponent>(entity));

		gpm.EmplaceComponent<fr::Bounds>(entity, PrivateCTORTag{});
		gpm.EmplaceOrReplaceComponent<DirtyMarker<fr::Bounds>>(entity);
	}


	void Bounds::AttachBoundsComponent(
		fr::GameplayManager& gpm, entt::entity entity, glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ)
	{
		SEAssert("Bounds can only be attached to an entity that already has a RenderDataComponent",
			fr::Relationship::HasComponentInParentHierarchy<gr::RenderDataComponent>(entity));

		gpm.EmplaceComponent<fr::Bounds>(entity, PrivateCTORTag{}, minXYZ, maxXYZ);
		gpm.EmplaceOrReplaceComponent<DirtyMarker<fr::Bounds>>(entity);
	}


	void Bounds::AttachBoundsComponent(
		fr::GameplayManager& gpm,
		entt::entity entity,
		glm::vec3 const& minXYZ,
		glm::vec3 const& maxXYZ,
		std::vector<glm::vec3> const& positions)
	{
		SEAssert("Bounds can only be attached to an entity that already has a RenderDataComponent", 
			fr::Relationship::HasComponentInParentHierarchy<gr::RenderDataComponent>(entity));

		gpm.EmplaceComponent<fr::Bounds>(entity, PrivateCTORTag{}, minXYZ, maxXYZ, positions);
		gpm.EmplaceOrReplaceComponent<DirtyMarker<fr::Bounds>>(entity);
	}


	gr::Bounds::RenderData Bounds::CreateRenderData(fr::Bounds const& bounds)
	{
		return gr::Bounds::RenderData
		{
			.m_minXYZ = bounds.m_minXYZ,
			.m_maxXYZ = bounds.m_maxXYZ
		};
	}


	Bounds::Bounds(PrivateCTORTag)
		: m_minXYZ(k_invalidMinXYZ)
		, m_maxXYZ(k_invalidMaxXYZ) 
	{
	}


	Bounds::Bounds()
		: Bounds(PrivateCTORTag{})
	{
	}


	Bounds::Bounds(PrivateCTORTag, glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ)
		: m_minXYZ(minXYZ)
		, m_maxXYZ(maxXYZ)
	{
	}


	Bounds::Bounds(
		PrivateCTORTag, glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ, std::vector<glm::vec3> const& positions)
		: m_minXYZ(minXYZ)
		, m_maxXYZ(maxXYZ)
	{
		if (m_minXYZ == fr::Bounds::k_invalidMinXYZ || m_maxXYZ == fr::Bounds::k_invalidMaxXYZ)
		{
			// Legacy: Previously, we stored vertex data in vecN types. Instead of rewriting, just cast from floats
			ComputeBounds(positions);
		}
	}


	bool Bounds::operator==(fr::Bounds const& rhs) const
	{
		return m_minXYZ == rhs.m_minXYZ && m_maxXYZ == rhs.m_maxXYZ;
	}


	bool Bounds::operator!=(fr::Bounds const& rhs) const
	{
		return operator==(rhs) == false;
	}


	// Returns a new AABB BoundsConcept, transformed from local space using transform
	Bounds Bounds::GetTransformedAABBBounds(mat4 const& worldMatrix) const
	{
		// Assemble our current AABB points into a cube of 8 vertices:
		std::vector<vec4>points(8);							// "front" == fwd == Z -
		points[0] = vec4(xMin(), yMax(), zMin(), 1.0f);		// Left		top		front 
		points[1] = vec4(xMax(), yMax(), zMin(), 1.0f);		// Right	top		front
		points[2] = vec4(xMin(), yMin(), zMin(), 1.0f);		// Left		bot		front
		points[3] = vec4(xMax(), yMin(), zMin(), 1.0f);		// Right	bot		
		points[4] = vec4(xMin(), yMax(), zMax(), 1.0f);		// Left		top		back
		points[5] = vec4(xMax(), yMax(), zMax(), 1.0f);		// Right	top		back
		points[6] = vec4(xMin(), yMin(), zMax(), 1.0f);		// Left		bot		back
		points[7] = vec4(xMax(), yMin(), zMax(), 1.0f);		// Right	bot		back

		// Compute a new AABB in world-space:
		Bounds result(PrivateCTORTag{}); // Invalid min/max by default

		// Transform each point into world space, and record the min/max coordinate in each dimension:
		for (size_t i = 0; i < 8; i++)
		{
			points[i] = worldMatrix * points[i];

			result.m_minXYZ.x = std::min(points[i].x, result.m_minXYZ.x );
			result.m_maxXYZ.x = std::max(points[i].x, result.m_maxXYZ.x);

			result.m_minXYZ.y = std::min(points[i].y, result.m_minXYZ.y);
			result.m_maxXYZ.y = std::max(points[i].y, result.m_maxXYZ.y);

			result.m_minXYZ.z = std::min(points[i].z, result.m_minXYZ.z);
			result.m_maxXYZ.z = std::max(points[i].z, result.m_maxXYZ.z);
		}

		result.Make3Dimensional(); // Ensure the final bounds are 3D

		return result;
	}


	void Bounds::ComputeBounds(std::vector<glm::vec3> const& positions)
	{
		for (size_t i = 0; i < positions.size(); i++)
		{
			m_minXYZ.x = std::min(positions[i].x, m_minXYZ.x);
			m_maxXYZ.x = std::max(positions[i].x, m_maxXYZ.x);

			m_minXYZ.y = std::min(positions[i].y, m_minXYZ.y);
			m_maxXYZ.y = std::max(positions[i].y, m_maxXYZ.y);

			m_minXYZ.z = std::min(positions[i].z, m_minXYZ.z);
			m_maxXYZ.z = std::max(positions[i].z, m_maxXYZ.z);
		}
	}


	void Bounds::ExpandBounds(Bounds const& newContents)
	{
		m_minXYZ.x = std::min(newContents.m_minXYZ.x, m_minXYZ.x);
		m_maxXYZ.x = std::max(newContents.m_maxXYZ.x, m_maxXYZ.x);

		m_minXYZ.y = std::min(newContents.m_minXYZ.y, m_minXYZ.y);
		m_maxXYZ.y = std::max(newContents.m_maxXYZ.y, m_maxXYZ.y);
		
		m_minXYZ.z = std::min(newContents.m_minXYZ.z, m_minXYZ.z);
		m_maxXYZ.z = std::max(newContents.m_maxXYZ.z, m_maxXYZ.z);
	}


	void Bounds::Make3Dimensional()
	{
		if (glm::abs(m_maxXYZ.x - m_minXYZ.x) < k_bounds3DDepthBias)
		{
			m_minXYZ.x -= k_bounds3DDepthBias;
			m_maxXYZ.x += k_bounds3DDepthBias;
		}

		if (glm::abs(m_maxXYZ.y - m_minXYZ.y) < k_bounds3DDepthBias)
		{
			m_minXYZ.y -= k_bounds3DDepthBias;
			m_maxXYZ.y += k_bounds3DDepthBias;
		}

		if (glm::abs(m_maxXYZ.z - m_minXYZ.z) < k_bounds3DDepthBias)
		{
			m_minXYZ.z -= k_bounds3DDepthBias;
			m_maxXYZ.z += k_bounds3DDepthBias;
		}
	}


	void Bounds::ShowImGuiWindow() const
	{
		ImGui::Text("Min XYZ = %s", glm::to_string(m_minXYZ).c_str());
		ImGui::Text("Max XYZ = %s", glm::to_string(m_maxXYZ).c_str());
	}
}