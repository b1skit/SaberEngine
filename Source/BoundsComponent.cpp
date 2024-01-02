// © 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "Transform.h"


namespace
{
	constexpr float k_bounds3DDepthBias = 0.01f; // Offset to ensure axis min != axis max
}

namespace fr
{
	void BoundsComponent::CreateSceneBoundsConcept(fr::EntityManager& em)
	{
		constexpr char const* k_sceneBoundsName = "SceneBounds";

		entt::entity sceneBoundsEntity = em.CreateEntity(k_sceneBoundsName);

		// Create a Transform and render data representation: 
		fr::TransformComponent& sceneBoundsTransformComponent = 
			fr::TransformComponent::AttachTransformComponent(em, sceneBoundsEntity, nullptr);
		
		gr::RenderDataComponent::AttachNewRenderDataComponent(
			em, sceneBoundsEntity, sceneBoundsTransformComponent.GetTransformID());

		em.EmplaceComponent<SceneBoundsMarker>(sceneBoundsEntity);

		// Attach the BoundsComponent:
		AttachBoundsComponent(em, sceneBoundsEntity);
	}


	void BoundsComponent::AttachBoundsComponent(
		fr::EntityManager& em, entt::entity entity)
	{
		// Finally, attach the BoundsComponent (which will trigger event listeners)
		em.EmplaceComponent<fr::BoundsComponent>(entity, PrivateCTORTag{});

		em.EmplaceComponent<DirtyMarker<fr::BoundsComponent>>(entity);
	}


	void BoundsComponent::AttachBoundsComponent(
		fr::EntityManager& em,
		entt::entity entity, 
		glm::vec3 const& minXYZ,
		glm::vec3 const& maxXYZ)
	{
		// Finally, attach the BoundsComponent (which will trigger event listeners)
		em.EmplaceComponent<fr::BoundsComponent>(entity, PrivateCTORTag{}, minXYZ, maxXYZ);

		em.EmplaceComponent<DirtyMarker<fr::BoundsComponent>>(entity);
	}


	void BoundsComponent::AttachBoundsComponent(
		fr::EntityManager& em,
		entt::entity entity,
		glm::vec3 const& minXYZ,
		glm::vec3 const& maxXYZ,
		std::vector<glm::vec3> const& positions)
	{
		// Finally, attach the BoundsComponent (which will trigger event listeners)
		em.EmplaceComponent<fr::BoundsComponent>(entity, PrivateCTORTag{}, minXYZ, maxXYZ, positions);

		em.EmplaceComponent<DirtyMarker<fr::BoundsComponent>>(entity);
	}


	gr::Bounds::RenderData BoundsComponent::CreateRenderData(fr::BoundsComponent const& bounds, fr::NameComponent const&)
	{
		return gr::Bounds::RenderData
		{
			.m_minXYZ = bounds.m_minXYZ,
			.m_maxXYZ = bounds.m_maxXYZ
		};
	}


	void BoundsComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity owningEntity)
	{
		if (ImGui::CollapsingHeader(
			std::format("Local bounds:##{}", static_cast<uint32_t>(owningEntity)).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			gr::RenderDataComponent::ShowImGuiWindow(em, owningEntity);

			fr::BoundsComponent const& boundsCmpt = em.GetComponent<fr::BoundsComponent>(owningEntity);
			
			ImGui::Text("Min XYZ = %s", glm::to_string(boundsCmpt.m_minXYZ).c_str());
			ImGui::Text("Max XYZ = %s", glm::to_string(boundsCmpt.m_maxXYZ).c_str());

			ImGui::Unindent();
		}
	}


	BoundsComponent::BoundsComponent(PrivateCTORTag)
		: m_minXYZ(k_invalidMinXYZ)
		, m_maxXYZ(k_invalidMaxXYZ) 
	{
	}


	BoundsComponent::BoundsComponent()
		: BoundsComponent(PrivateCTORTag{})
	{
	}


	BoundsComponent::BoundsComponent(PrivateCTORTag, glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ)
		: m_minXYZ(minXYZ)
		, m_maxXYZ(maxXYZ)
	{
		SEAssert("Cannot have only 1 invalid minXYZ/maxXYZ", 
			(m_minXYZ == BoundsComponent::k_invalidMinXYZ && m_maxXYZ == BoundsComponent::k_invalidMaxXYZ) ||
			(m_minXYZ != BoundsComponent::k_invalidMinXYZ && m_maxXYZ != BoundsComponent::k_invalidMaxXYZ));
		SEAssert("Bounds is NaN/Inf",
			glm::all(glm::isnan(m_minXYZ)) == false && glm::all(glm::isnan(m_maxXYZ)) == false &&
			glm::all(glm::isinf(m_minXYZ)) == false && glm::all(glm::isinf(m_maxXYZ)) == false);
	}


	BoundsComponent::BoundsComponent(
		PrivateCTORTag, glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ, std::vector<glm::vec3> const& positions)
		: m_minXYZ(minXYZ)
		, m_maxXYZ(maxXYZ)
	{
		SEAssert("Cannot have only 1 invalid minXYZ/maxXYZ",
			(m_minXYZ == BoundsComponent::k_invalidMinXYZ && m_maxXYZ == BoundsComponent::k_invalidMaxXYZ) ||
			(m_minXYZ != BoundsComponent::k_invalidMinXYZ && m_maxXYZ != BoundsComponent::k_invalidMaxXYZ));

		if (m_minXYZ == fr::BoundsComponent::k_invalidMinXYZ || m_maxXYZ == fr::BoundsComponent::k_invalidMaxXYZ)
		{
			ComputeBounds(positions);
		}
	}


	bool BoundsComponent::operator==(fr::BoundsComponent const& rhs) const
	{
		return m_minXYZ == rhs.m_minXYZ && m_maxXYZ == rhs.m_maxXYZ;
	}


	bool BoundsComponent::operator!=(fr::BoundsComponent const& rhs) const
	{
		return operator==(rhs) == false;
	}


	// Returns a new AABB BoundsConcept, transformed from local space using transform
	BoundsComponent BoundsComponent::GetTransformedAABBBounds(glm::mat4 const& worldMatrix) const
	{
		// Assemble our current AABB points into a cube of 8 vertices:
		std::vector<glm::vec4>points(8);							// "front" == fwd == Z -
		points[0] = glm::vec4(xMin(), yMax(), zMin(), 1.0f);		// Left		top		front 
		points[1] = glm::vec4(xMax(), yMax(), zMin(), 1.0f);		// Right	top		front
		points[2] = glm::vec4(xMin(), yMin(), zMin(), 1.0f);		// Left		bot		front
		points[3] = glm::vec4(xMax(), yMin(), zMin(), 1.0f);		// Right	bot		
		points[4] = glm::vec4(xMin(), yMax(), zMax(), 1.0f);		// Left		top		back
		points[5] = glm::vec4(xMax(), yMax(), zMax(), 1.0f);		// Right	top		back
		points[6] = glm::vec4(xMin(), yMin(), zMax(), 1.0f);		// Left		bot		back
		points[7] = glm::vec4(xMax(), yMin(), zMax(), 1.0f);		// Right	bot		back

		// Compute a new AABB in world-space:
		BoundsComponent result(PrivateCTORTag{}); // Invalid min/max by default

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

		SEAssert("Bounds is NaN/Inf", 
			glm::all(glm::isnan(result.m_minXYZ)) == false && glm::all(glm::isnan(result.m_maxXYZ)) == false &&
			glm::all(glm::isinf(result.m_minXYZ)) == false && glm::all(glm::isinf(result.m_maxXYZ)) == false);

		return result;
	}


	void BoundsComponent::ComputeBounds(std::vector<glm::vec3> const& positions)
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
		SEAssert("Bounds is NaN/Inf",
			glm::all(glm::isnan(m_minXYZ)) == false && glm::all(glm::isnan(m_maxXYZ)) == false &&
			glm::all(glm::isinf(m_minXYZ)) == false && glm::all(glm::isinf(m_maxXYZ)) == false);
	}


	void BoundsComponent::ExpandBounds(BoundsComponent const& newContents)
	{
		m_minXYZ.x = std::min(newContents.m_minXYZ.x, m_minXYZ.x);
		m_maxXYZ.x = std::max(newContents.m_maxXYZ.x, m_maxXYZ.x);

		m_minXYZ.y = std::min(newContents.m_minXYZ.y, m_minXYZ.y);
		m_maxXYZ.y = std::max(newContents.m_maxXYZ.y, m_maxXYZ.y);
		
		m_minXYZ.z = std::min(newContents.m_minXYZ.z, m_minXYZ.z);
		m_maxXYZ.z = std::max(newContents.m_maxXYZ.z, m_maxXYZ.z);

		SEAssert("Bounds is NaN/Inf",
			glm::all(glm::isnan(m_minXYZ)) == false && glm::all(glm::isnan(m_maxXYZ)) == false &&
			glm::all(glm::isinf(m_minXYZ)) == false && glm::all(glm::isinf(m_maxXYZ)) == false);
	}


	void BoundsComponent::ExpandBoundsHierarchy(
		fr::EntityManager& em, BoundsComponent const& newContents, entt::entity boundsEntity)
	{
		ExpandBounds(newContents);

		SEAssert("Owning entity does not have a Relationship component", 
			em.HasComponent<fr::Relationship>(boundsEntity));

		fr::Relationship const& owningEntityRelationship = em.GetComponent<fr::Relationship>(boundsEntity);

		// Recursively expand any Bounds above us:
		entt::entity nextEntity = entt::null;
		fr::BoundsComponent* nextBounds = em.GetFirstAndEntityInHierarchyAbove<fr::BoundsComponent>(
			owningEntityRelationship.GetParent(), 
			nextEntity);
		if (nextBounds != nullptr)
		{
			ExpandBoundsHierarchy(em, *this, nextEntity);
		}
	}


	void BoundsComponent::Make3Dimensional()
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

		SEAssert("Bounds is NaN/Inf",
			glm::all(glm::isnan(m_minXYZ)) == false && glm::all(glm::isnan(m_maxXYZ)) == false &&
			glm::all(glm::isinf(m_minXYZ)) == false && glm::all(glm::isinf(m_maxXYZ)) == false);
	}
}