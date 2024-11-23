// © 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "TransformComponent.h"

#include "Core/Util/ByteVector.h"


namespace
{
	constexpr float k_bounds3DDepthBias = 0.01f; // Offset to ensure axis min != axis max


	void ConfigureEncapsulatingBoundsRenderDataID(
		fr::EntityManager& em, entt::entity owningEntity, fr::BoundsComponent& bounds)
	{
		fr::Relationship const& owningEntityRelationship = em.GetComponent<fr::Relationship>(owningEntity);

		// Recursively expand any Bounds above us:
		if (owningEntityRelationship.HasParent())
		{
			fr::Relationship const& parentRelationship = 
				em.GetComponent<fr::Relationship>(owningEntityRelationship.GetParent());

			entt::entity nextEntity = entt::null;
			fr::BoundsComponent* nextBounds =
				parentRelationship.GetFirstAndEntityInHierarchyAbove<fr::BoundsComponent>(nextEntity);

			if (nextBounds != nullptr)
			{
				fr::RenderDataComponent const& nextBoundsRenderDataCmpt = 
					em.GetComponent<fr::RenderDataComponent>(nextEntity);

				bounds.SetEncapsulatingBoundsRenderDataID(nextBoundsRenderDataCmpt.GetRenderDataID());
			}
		}
		else
		{
			bounds.SetEncapsulatingBoundsRenderDataID(gr::k_invalidRenderDataID);
		}
	}


	void ValidateMinMaxBounds(glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ)
	{
#if defined(_DEBUG)
		SEAssert((minXYZ != fr::BoundsComponent::k_invalidMinXYZ && 
			maxXYZ != fr::BoundsComponent::k_invalidMaxXYZ),
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


namespace fr
{
	void BoundsComponent::CreateSceneBoundsConcept(fr::EntityManager& em)
	{
		constexpr char const* k_sceneBoundsName = "SceneBounds";

		entt::entity sceneBoundsEntity = em.CreateEntity(k_sceneBoundsName);

		// Create a Transform and render data representation: 
		fr::TransformComponent& sceneBoundsTransformComponent = 
			fr::TransformComponent::AttachTransformComponent(em, sceneBoundsEntity);

		SEAssert(sceneBoundsTransformComponent.GetTransform().GetParent() == nullptr,
			"Found a parent transform for the scene bounds. This is unexpected");
		
		fr::RenderDataComponent* sceneBoundsRenderCmpt = fr::RenderDataComponent::GetCreateRenderDataComponent(
			em, sceneBoundsEntity, sceneBoundsTransformComponent.GetTransformID());

		sceneBoundsRenderCmpt->SetFeatureBit(gr::RenderObjectFeature::IsSceneBounds);

		em.EmplaceComponent<SceneBoundsMarker>(sceneBoundsEntity);

		// Attach the BoundsComponent:
		AttachBoundsComponent(em, sceneBoundsEntity);
	}


	void BoundsComponent::AttachBoundsComponent(fr::EntityManager& em, entt::entity entity)
	{
		SEAssert(em.GetComponent<fr::Relationship>(entity).IsInHierarchyAbove<fr::TransformComponent>(),
			"A Bounds requires a TransformComponent");

		// Attach the BoundsComponent (which will trigger event listeners)
		fr::BoundsComponent* boundsCmpt = em.EmplaceComponent<fr::BoundsComponent>(entity, PrivateCTORTag{});

		ConfigureEncapsulatingBoundsRenderDataID(em, entity, *boundsCmpt);

		em.EmplaceComponent<DirtyMarker<fr::BoundsComponent>>(entity);
	}


	void BoundsComponent::AttachBoundsComponent(
		fr::EntityManager& em,
		entt::entity entity, 
		glm::vec3 const& minXYZ,
		glm::vec3 const& maxXYZ)
	{
		SEAssert(em.GetComponent<fr::Relationship>(entity).IsInHierarchyAbove<fr::TransformComponent>(),
			"A Bounds requires a TransformComponent");

		// Attach the BoundsComponent (which will trigger event listeners)
		fr::BoundsComponent* boundsCmpt = 
			em.EmplaceComponent<fr::BoundsComponent>(entity, PrivateCTORTag{}, minXYZ, maxXYZ);

		ConfigureEncapsulatingBoundsRenderDataID(em, entity, *boundsCmpt);

		em.EmplaceComponent<DirtyMarker<fr::BoundsComponent>>(entity);
	}


	gr::Bounds::RenderData BoundsComponent::CreateRenderData(
		entt::entity owningEntity, fr::BoundsComponent const& bounds)
	{
		fr::EntityManager const* em = fr::EntityManager::Get();;

		glm::vec3 globalMinXYZ = bounds.m_localMinXYZ;
		glm::vec3 globalMaxXYZ = bounds.m_localMaxXYZ;
		if (!em->HasComponent<SceneBoundsMarker>(owningEntity))
		{
			fr::TransformComponent const* transformCmpt = 
				em->GetComponent<fr::Relationship>(owningEntity).GetFirstInHierarchyAbove<fr::TransformComponent>();

			BoundsComponent const& globalBounds =
				bounds.GetTransformedAABBBounds(transformCmpt->GetTransform().GetGlobalMatrix());

			globalMinXYZ = globalBounds.m_localMinXYZ;
			globalMaxXYZ = globalBounds.m_localMaxXYZ;
		}
		
		return gr::Bounds::RenderData
		{
			.m_encapsulatingBounds = bounds.GetEncapsulatingBoundsRenderDataID(),

			.m_localMinXYZ = bounds.m_localMinXYZ,
			.m_localMaxXYZ = bounds.m_localMaxXYZ,

			.m_globalMinXYZ = globalMinXYZ,
			.m_globalMaxXYZ = globalMaxXYZ,
		};
	}


	BoundsComponent::BoundsComponent(PrivateCTORTag)
		: m_localMinXYZ(k_invalidMinXYZ)
		, m_localMaxXYZ(k_invalidMaxXYZ)
		, m_encapsulatingBoundsRenderDataID(gr::k_invalidRenderDataID)
	{
		// Note: The bounds must be set to something valid i.e. by expanding when a child is attached
	}


	BoundsComponent::BoundsComponent(PrivateCTORTag, glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ)
		: m_localMinXYZ(minXYZ)
		, m_localMaxXYZ(maxXYZ)
		, m_encapsulatingBoundsRenderDataID(gr::k_invalidRenderDataID)
	{
		Make3Dimensional();

		ValidateMinMaxBounds(m_localMinXYZ, m_localMaxXYZ); // _DEBUG only
	}


	bool BoundsComponent::operator==(fr::BoundsComponent const& rhs) const
	{
		return m_localMinXYZ == rhs.m_localMinXYZ && m_localMaxXYZ == rhs.m_localMaxXYZ;
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

			result.m_localMinXYZ.x = std::min(points[i].x, result.m_localMinXYZ.x );
			result.m_localMaxXYZ.x = std::max(points[i].x, result.m_localMaxXYZ.x);

			result.m_localMinXYZ.y = std::min(points[i].y, result.m_localMinXYZ.y);
			result.m_localMaxXYZ.y = std::max(points[i].y, result.m_localMaxXYZ.y);

			result.m_localMinXYZ.z = std::min(points[i].z, result.m_localMinXYZ.z);
			result.m_localMaxXYZ.z = std::max(points[i].z, result.m_localMaxXYZ.z);
		}

		result.Make3Dimensional(); // Ensure the final bounds are 3D

		ValidateMinMaxBounds(m_localMinXYZ, m_localMaxXYZ); // _DEBUG only

		return result;
	}


	void BoundsComponent::ExpandBounds(BoundsComponent const& newContents)
	{
		m_localMinXYZ.x = std::min(newContents.m_localMinXYZ.x, m_localMinXYZ.x);
		m_localMaxXYZ.x = std::max(newContents.m_localMaxXYZ.x, m_localMaxXYZ.x);

		m_localMinXYZ.y = std::min(newContents.m_localMinXYZ.y, m_localMinXYZ.y);
		m_localMaxXYZ.y = std::max(newContents.m_localMaxXYZ.y, m_localMaxXYZ.y);
		
		m_localMinXYZ.z = std::min(newContents.m_localMinXYZ.z, m_localMinXYZ.z);
		m_localMaxXYZ.z = std::max(newContents.m_localMaxXYZ.z, m_localMaxXYZ.z);

		ValidateMinMaxBounds(m_localMinXYZ, m_localMaxXYZ); // _DEBUG only
	}


	void BoundsComponent::ExpandBoundsHierarchy(
		fr::EntityManager& em, BoundsComponent const& newContents, entt::entity boundsEntity)
	{
		ExpandBounds(newContents);

		SEAssert(em.HasComponent<fr::Relationship>(boundsEntity),
			"Owning entity does not have a Relationship component");

		fr::Relationship const& owningEntityRelationship = em.GetComponent<fr::Relationship>(boundsEntity);

		// Recursively expand any Bounds above us:
		if (owningEntityRelationship.HasParent())
		{
			fr::Relationship const& parentRelationship =
				em.GetComponent<fr::Relationship>(owningEntityRelationship.GetParent());

			entt::entity nextEntity = entt::null;
			fr::BoundsComponent* nextBounds =
				parentRelationship.GetFirstAndEntityInHierarchyAbove<fr::BoundsComponent>(nextEntity);

			if (nextBounds != nullptr)
			{
				ExpandBoundsHierarchy(em, *this, nextEntity);
			}
		}
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


	void BoundsComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity owningEntity, bool startOpen /*= false*/)
	{
		const ImGuiTreeNodeFlags_ flags = startOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;

		if (ImGui::CollapsingHeader(
			std::format("Local bounds##{}", static_cast<uint32_t>(owningEntity)).c_str(), flags))
		{
			ImGui::Indent();

			// RenderDataComponent:
			fr::RenderDataComponent::ShowImGuiWindow(em, owningEntity);

			fr::BoundsComponent const& boundsCmpt = em.GetComponent<fr::BoundsComponent>(owningEntity);

			ImGui::Text("Min XYZ = %s", glm::to_string(boundsCmpt.m_localMinXYZ).c_str());
			ImGui::Text("Max XYZ = %s", glm::to_string(boundsCmpt.m_localMaxXYZ).c_str());

			ImGui::Unindent();
		}
	}
}