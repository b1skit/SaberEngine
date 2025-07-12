// © 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "TransformComponent.h"


namespace
{
	constexpr float k_bounds3DDepthBias = 0.01f; // Offset to ensure axis min != axis max


	void ConfigureEncapsulatingBoundsRenderDataID(
		pr::EntityManager& em, entt::entity boundsEntity, pr::BoundsComponent& bounds, entt::entity encapsulatingBounds)
	{
		if (encapsulatingBounds == entt::null)
		{
			return;
		}
		SEAssert(em.HasComponent<pr::BoundsComponent>(encapsulatingBounds),
			"Encapsulating bounds entity does not have a BoundsComponent");

		pr::RenderDataComponent const& encapsulatingRenderDataCmpt =
			em.GetComponent<pr::RenderDataComponent>(encapsulatingBounds);

		bounds.SetEncapsulatingBounds(encapsulatingBounds, encapsulatingRenderDataCmpt.GetRenderDataID());
	}


	void ComputeWorldMinMax(
		pr::EntityManager& em,
		entt::entity boundsEntity,
		pr::BoundsComponent const& bounds,
		glm::vec3& globalMinXYZOut,
		glm::vec3& globalMaxXYZOut)
	{
		globalMinXYZOut = bounds.GetLocalMinXYZ();
		globalMaxXYZOut = bounds.GetLocalMaxXYZ();
		if (!em.HasComponent<pr::BoundsComponent::SceneBoundsMarker>(boundsEntity))
		{
			pr::TransformComponent const* transformCmpt =
				em.GetComponent<pr::Relationship>(boundsEntity).GetFirstInHierarchyAbove<pr::TransformComponent>();

			pr::BoundsComponent const& globalBounds =
				bounds.GetTransformedAABBBounds(transformCmpt->GetTransform().GetGlobalMatrix());

			globalMinXYZOut = globalBounds.GetLocalMinXYZ();
			globalMaxXYZOut = globalBounds.GetLocalMaxXYZ();
		}
	}


	void ValidateMinMaxBounds(glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ)
	{
#if defined(_DEBUG)
		SEAssert((minXYZ != pr::BoundsComponent::k_invalidMinXYZ && 
			maxXYZ != pr::BoundsComponent::k_invalidMaxXYZ),
			"Invalid minXYZ/maxXYZ");

		SEAssert(minXYZ != maxXYZ &&
			minXYZ.x < maxXYZ.x &&
			minXYZ.y < maxXYZ.y &&
			minXYZ.z < maxXYZ.z,
			"Invalid min/max positions");

		SEAssert(glm::all(glm::isnan(minXYZ)) == false && glm::all(glm::isnan(maxXYZ)) == false &&
			glm::all(glm::isinf(minXYZ)) == false && glm::all(glm::isinf(maxXYZ)) == false,
			"Bounds is NaN/Inf");
#endif
	}
}


namespace pr
{
	pr::BoundsComponent& BoundsComponent::CreateSceneBoundsConcept(pr::EntityManager& em)
	{
		constexpr char const* k_sceneBoundsName = "SceneBounds";

		entt::entity sceneBoundsEntity = em.CreateEntity(k_sceneBoundsName);

		// Create a Transform and render data representation: 
		pr::TransformComponent& sceneBoundsTransformComponent = 
			pr::TransformComponent::AttachTransformComponent(em, sceneBoundsEntity);

		SEAssert(sceneBoundsTransformComponent.GetTransform().GetParent() == nullptr,
			"Found a parent transform for the scene bounds. This is unexpected");
		
		pr::RenderDataComponent* sceneBoundsRenderCmpt = pr::RenderDataComponent::GetCreateRenderDataComponent(
			em, sceneBoundsEntity, sceneBoundsTransformComponent.GetTransformID());

		sceneBoundsRenderCmpt->SetFeatureBit(gr::RenderObjectFeature::IsSceneBounds);

		em.EmplaceComponent<SceneBoundsMarker>(sceneBoundsEntity);

		return AttachBoundsComponent(em, sceneBoundsEntity, entt::null);
	}


	pr::BoundsComponent& BoundsComponent::AttachBoundsComponent(
		pr::EntityManager& em, entt::entity entity, entt::entity encapsulatingBounds)
	{
		SEAssert(em.GetComponent<pr::Relationship>(entity).IsInHierarchyAbove<pr::TransformComponent>(),
			"A Bounds requires a TransformComponent");

		// Attach the BoundsComponent (which will trigger event listeners)
		pr::BoundsComponent* boundsCmpt = em.EmplaceComponent<pr::BoundsComponent>(entity, PrivateCTORTag{});

		ConfigureEncapsulatingBoundsRenderDataID(em, entity, *boundsCmpt, encapsulatingBounds);

		MarkDirty(entity);

		return *boundsCmpt;
	}


	pr::BoundsComponent& BoundsComponent::AttachBoundsComponent(
		pr::EntityManager& em,
		entt::entity entity,
		entt::entity encapsulatingBounds,
		glm::vec3 const& minXYZ,
		glm::vec3 const& maxXYZ)
	{
		SEAssert(em.GetComponent<pr::Relationship>(entity).IsInHierarchyAbove<pr::TransformComponent>(),
			"A Bounds requires a TransformComponent");

		// Attach the BoundsComponent (which will trigger event listeners)
		pr::BoundsComponent* boundsCmpt = 
			em.EmplaceComponent<pr::BoundsComponent>(entity, PrivateCTORTag{}, minXYZ, maxXYZ, encapsulatingBounds);

		ConfigureEncapsulatingBoundsRenderDataID(em, entity, *boundsCmpt, encapsulatingBounds);

		boundsCmpt->ExpandEncapsulatingBounds(em, *boundsCmpt, entity);

		MarkDirty(entity);

		return *boundsCmpt;
	}


	void BoundsComponent::UpdateBoundsComponent(
		pr::EntityManager& em,
		pr::BoundsComponent& boundsCmpt,
		pr::Relationship const& relationship,
		entt::entity boundsEntity)
	{
		if (pr::TransformComponent const* transformCmpt = relationship.GetFirstInHierarchyAbove<pr::TransformComponent>())
		{
			if (transformCmpt->GetTransform().HasChanged())
			{
				boundsCmpt.MarkDirty(boundsEntity);
			}
		}

		std::vector<entt::entity> const& childBounds = 
			relationship.GetAllEntitiesInImmediateChildren<pr::BoundsComponent>();
		for (entt::entity child : childBounds)
		{
			pr::BoundsComponent& childBounds = em.GetComponent<pr::BoundsComponent>(child);
			if (childBounds.GetEncapsulatingBoundsEntity() == boundsEntity)
			{
				// Internally calls MarkDirty()
				boundsCmpt.ExpandBoundsInternal(childBounds.m_localMinXYZ, childBounds.m_localMaxXYZ, boundsEntity);
			}
		}
	}


	gr::Bounds::RenderData BoundsComponent::CreateRenderData(
		entt::entity owningEntity, pr::BoundsComponent const& bounds)
	{
		glm::vec3 worldMinXYZ;
		glm::vec3 worldMaxXYZ;
		ComputeWorldMinMax(*pr::EntityManager::Get(), owningEntity, bounds, worldMinXYZ, worldMaxXYZ);
		
		return gr::Bounds::RenderData
		{
			.m_encapsulatingBounds = bounds.GetEncapsulatingBoundsRenderDataID(),

			.m_localMinXYZ = bounds.m_localMinXYZ,
			.m_localMaxXYZ = bounds.m_localMaxXYZ,

			.m_worldMinXYZ = worldMinXYZ,
			.m_worldMaxXYZ = worldMaxXYZ,
		};
	}


	BoundsComponent::BoundsComponent(PrivateCTORTag)
		: m_localMinXYZ(k_invalidMinXYZ)
		, m_localMaxXYZ(k_invalidMaxXYZ)
		, m_originalMinXYZ(k_invalidMinXYZ)
		, m_originalMaxXYZ(k_invalidMaxXYZ)
		, m_encapsulatingBoundsEntity(entt::null)
		, m_encapsulatingBoundsRenderDataID(gr::k_invalidRenderDataID)
	{
		// Note: The bounds must be set to something valid i.e. by expanding when a child is attached
	}


	BoundsComponent::BoundsComponent(
		PrivateCTORTag, glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ, entt::entity encapsulatingBounds)
		: m_localMinXYZ(minXYZ)
		, m_localMaxXYZ(maxXYZ)
		, m_originalMinXYZ(minXYZ)
		, m_originalMaxXYZ(maxXYZ)
		, m_encapsulatingBoundsEntity(encapsulatingBounds)
		, m_encapsulatingBoundsRenderDataID(gr::k_invalidRenderDataID)
	{
		Make3Dimensional();

		ValidateMinMaxBounds(m_localMinXYZ, m_localMaxXYZ); // _DEBUG only
	}


	bool BoundsComponent::operator==(pr::BoundsComponent const& rhs) const
	{
		return m_localMinXYZ == rhs.m_localMinXYZ && m_localMaxXYZ == rhs.m_localMaxXYZ;
	}


	bool BoundsComponent::operator!=(pr::BoundsComponent const& rhs) const
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

			result.m_localMinXYZ.x = std::min(points[i].x, result.m_localMinXYZ.x );
			result.m_localMaxXYZ.x = std::max(points[i].x, result.m_localMaxXYZ.x);

			result.m_localMinXYZ.y = std::min(points[i].y, result.m_localMinXYZ.y);
			result.m_localMaxXYZ.y = std::max(points[i].y, result.m_localMaxXYZ.y);

			result.m_localMinXYZ.z = std::min(points[i].z, result.m_localMinXYZ.z);
			result.m_localMaxXYZ.z = std::max(points[i].z, result.m_localMaxXYZ.z);
		}

		result.Make3Dimensional(); // Ensure the final bounds are 3D

		ValidateMinMaxBounds(result.m_localMinXYZ, result.m_localMaxXYZ); // _DEBUG only

		return result;
	}


	void BoundsComponent::MarkDirty(entt::entity boundsEntity)
	{
		pr::EntityManager::Get()->TryEmplaceComponent<DirtyMarker<pr::BoundsComponent>>(boundsEntity);
	}


	void BoundsComponent::SetLocalMinXYZ(glm::vec3 const& newLocalMinXYZ, entt::entity boundsEntity)
	{
		SetLocalMinMaxXYZ(newLocalMinXYZ, m_localMaxXYZ, boundsEntity);
	}


	void BoundsComponent::SetLocalMaxXYZ(glm::vec3 const& newLocalMaxXYZ, entt::entity boundsEntity)
	{
		SetLocalMinMaxXYZ(m_localMinXYZ, newLocalMaxXYZ, boundsEntity);
	}


	void BoundsComponent::SetLocalMinMaxXYZ(
		glm::vec3 const& newLocalMinXYZ, glm::vec3 const& newLocalMaxXYZ, entt::entity boundsEntity)
	{
		ExpandBounds(newLocalMinXYZ, newLocalMaxXYZ, boundsEntity);
	}


	void BoundsComponent::ExpandBounds(BoundsComponent const& newContents, entt::entity boundsEntity)
	{
		ExpandBounds(newContents.m_localMinXYZ, newContents.m_localMaxXYZ, boundsEntity);
	}


	void BoundsComponent::ExpandBounds(glm::vec3 const& newMinXYZ, glm::vec3 const& newMaxXYZ, entt::entity boundsEntity)
	{
		ExpandBoundsInternal(newMinXYZ, newMaxXYZ, boundsEntity);
	}


	void BoundsComponent::ExpandEncapsulatingBounds(
		pr::EntityManager& em, BoundsComponent const& newContents, entt::entity boundsEntity)
	{
		ExpandEncapsulatingBounds(em, newContents.m_localMinXYZ, newContents.m_localMaxXYZ, boundsEntity);
	}


	void BoundsComponent::ExpandEncapsulatingBounds(
		pr::EntityManager& em,
		glm::vec3 const& newLocalMinXYZ,
		glm::vec3 const& newLocalMaxXYZ,
		entt::entity boundsEntity)
	{
		if (m_encapsulatingBoundsEntity != entt::null)
		{
			pr::BoundsComponent& encapsulatingBounds = em.GetComponent<pr::BoundsComponent>(m_encapsulatingBoundsEntity);
			encapsulatingBounds.ExpandBounds(newLocalMinXYZ, newLocalMaxXYZ, boundsEntity);
		}
	}


	bool BoundsComponent::ExpandBoundsInternal(
		glm::vec3 const& newMinXYZ, glm::vec3 const& newMaxXYZ, entt::entity boundsEntity)
	{
		bool isDirty = false;

		if (m_originalMinXYZ == k_invalidMinXYZ)
		{
			m_originalMinXYZ = newMinXYZ;
			isDirty = true;
		}

		if (m_originalMaxXYZ == k_invalidMaxXYZ)
		{
			m_originalMaxXYZ = newMaxXYZ;
			isDirty = true;
		}

		
		if (newMinXYZ.x < m_localMinXYZ.x)
		{
			m_localMinXYZ.x = newMinXYZ.x;
			isDirty = true;
		}
		if (newMaxXYZ.x > m_localMaxXYZ.x)
		{
			m_localMaxXYZ.x = newMaxXYZ.x;
			isDirty = true;
		}

		if (newMinXYZ.y < m_localMinXYZ.y)
		{
			m_localMinXYZ.y = newMinXYZ.y;
			isDirty = true;
		}
		if (newMaxXYZ.y > m_localMaxXYZ.y)
		{
			m_localMaxXYZ.y = newMaxXYZ.y;
			isDirty = true;
		}

		if (newMinXYZ.z < m_localMinXYZ.z)
		{
			m_localMinXYZ.z = newMinXYZ.z;
			isDirty = true;
		}
		if (newMaxXYZ.z > m_localMaxXYZ.z)
		{
			m_localMaxXYZ.z = newMaxXYZ.z;
			isDirty = true;
		}

		if (isDirty)
		{
			MarkDirty(boundsEntity);
		}

		ValidateMinMaxBounds(m_localMinXYZ, m_localMaxXYZ); // _DEBUG only

		return isDirty;
	}


	void BoundsComponent::Make3Dimensional()
	{
		if (glm::abs(m_localMaxXYZ.x - m_localMinXYZ.x) < k_bounds3DDepthBias)
		{
			m_localMinXYZ.x -= k_bounds3DDepthBias;
			m_localMaxXYZ.x += k_bounds3DDepthBias;
		}

		if (glm::abs(m_localMaxXYZ.y - m_localMinXYZ.y) < k_bounds3DDepthBias)
		{
			m_localMinXYZ.y -= k_bounds3DDepthBias;
			m_localMaxXYZ.y += k_bounds3DDepthBias;
		}

		if (glm::abs(m_localMaxXYZ.z - m_localMinXYZ.z) < k_bounds3DDepthBias)
		{
			m_localMinXYZ.z -= k_bounds3DDepthBias;
			m_localMaxXYZ.z += k_bounds3DDepthBias;
		}
	}


	void BoundsComponent::ShowImGuiWindow(pr::EntityManager& em, entt::entity owningEntity, bool startOpen /*= false*/)
	{
		const ImGuiTreeNodeFlags_ flags = startOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;

		if (ImGui::CollapsingHeader(
			std::format("Local bounds##{}", static_cast<uint32_t>(owningEntity)).c_str(), flags))
		{
			pr::RenderDataComponent::ShowImGuiWindow(em, owningEntity);

			ImGui::Indent();

			pr::NameComponent const& nameCmpt = em.GetComponent<pr::NameComponent>(owningEntity);
			ImGui::Text(std::format("\"{}\", entity: {}", nameCmpt.GetName(), static_cast<uint64_t>(owningEntity)).c_str());

			pr::BoundsComponent const& boundsCmpt = em.GetComponent<pr::BoundsComponent>(owningEntity);

			ImGui::Text(std::format("Encapsulting bounds entity: {}",
				static_cast<uint64_t>(boundsCmpt.m_encapsulatingBoundsEntity)).c_str());
			ImGui::Text(std::format("Encapsulting bounds RenderDataID: {}",
				boundsCmpt.m_encapsulatingBoundsRenderDataID).c_str());

			ImGui::Text("Original min XYZ = %s", glm::to_string(boundsCmpt.m_originalMinXYZ).c_str());
			ImGui::Text("Original max XYZ = %s", glm::to_string(boundsCmpt.m_originalMaxXYZ).c_str());

			ImGui::Text("Local min XYZ = %s", glm::to_string(boundsCmpt.m_localMinXYZ).c_str());
			ImGui::Text("Local max XYZ = %s", glm::to_string(boundsCmpt.m_localMaxXYZ).c_str());

			glm::vec3 worldMinXYZ;
			glm::vec3 worldMaxXYZ;
			ComputeWorldMinMax(*pr::EntityManager::Get(), owningEntity, boundsCmpt, worldMinXYZ, worldMaxXYZ);

			ImGui::Text("World min XYZ = %s", glm::to_string(worldMinXYZ).c_str());
			ImGui::Text("World max XYZ = %s", glm::to_string(worldMaxXYZ).c_str());

			ImGui::Unindent();
		}
	}
}