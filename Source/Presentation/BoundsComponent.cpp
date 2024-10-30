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
			entt::entity nextEntity = entt::null;

			fr::BoundsComponent* nextBounds = em.GetFirstAndEntityInHierarchyAbove<fr::BoundsComponent>(
				owningEntityRelationship.GetParent(),
				nextEntity);

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
		
		fr::RenderDataComponent& sceneBoundsRenderCmpt = fr::RenderDataComponent::AttachNewRenderDataComponent(
			em, sceneBoundsEntity, sceneBoundsTransformComponent.GetTransformID());

		sceneBoundsRenderCmpt.SetFeatureBit(gr::RenderObjectFeature::IsSceneBounds);

		em.EmplaceComponent<SceneBoundsMarker>(sceneBoundsEntity);

		// Attach the BoundsComponent:
		AttachBoundsComponent(em, sceneBoundsEntity);
	}


	void BoundsComponent::AttachBoundsComponent(fr::EntityManager& em, entt::entity entity)
	{
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
		// Attach the BoundsComponent (which will trigger event listeners)
		fr::BoundsComponent* boundsCmpt = 
			em.EmplaceComponent<fr::BoundsComponent>(entity, PrivateCTORTag{}, minXYZ, maxXYZ);

		ConfigureEncapsulatingBoundsRenderDataID(em, entity, *boundsCmpt);

		em.EmplaceComponent<DirtyMarker<fr::BoundsComponent>>(entity);
	}


	gr::Bounds::RenderData BoundsComponent::CreateRenderData(entt::entity, fr::BoundsComponent const& bounds)
	{
		return gr::Bounds::RenderData
		{
			.m_encapsulatingBounds = bounds.GetEncapsulatingBoundsRenderDataID(),

			.m_minXYZ = bounds.m_minXYZ,
			.m_maxXYZ = bounds.m_maxXYZ
		};
	}


	BoundsComponent::BoundsComponent(PrivateCTORTag)
		: m_minXYZ(k_invalidMinXYZ)
		, m_maxXYZ(k_invalidMaxXYZ)
		, m_encapsulatingBoundsRenderDataID(gr::k_invalidRenderDataID)
	{
		// Note: The bounds must be set to something valid i.e. by expanding when a child is attached
	}


	BoundsComponent::BoundsComponent(PrivateCTORTag, glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ)
		: m_minXYZ(minXYZ)
		, m_maxXYZ(maxXYZ)
		, m_encapsulatingBoundsRenderDataID(gr::k_invalidRenderDataID)
	{
		Make3Dimensional();

		ValidateMinMaxBounds(m_minXYZ, m_maxXYZ); // _DEBUG only
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

		ValidateMinMaxBounds(m_minXYZ, m_maxXYZ); // _DEBUG only

		return result;
	}


	void BoundsComponent::ExpandBounds(BoundsComponent const& newContents)
	{
		m_minXYZ.x = std::min(newContents.m_minXYZ.x, m_minXYZ.x);
		m_maxXYZ.x = std::max(newContents.m_maxXYZ.x, m_maxXYZ.x);

		m_minXYZ.y = std::min(newContents.m_minXYZ.y, m_minXYZ.y);
		m_maxXYZ.y = std::max(newContents.m_maxXYZ.y, m_maxXYZ.y);
		
		m_minXYZ.z = std::min(newContents.m_minXYZ.z, m_minXYZ.z);
		m_maxXYZ.z = std::max(newContents.m_maxXYZ.z, m_maxXYZ.z);

		ValidateMinMaxBounds(m_minXYZ, m_maxXYZ); // _DEBUG only
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
			entt::entity nextEntity = entt::null;

			fr::BoundsComponent* nextBounds = em.GetFirstAndEntityInHierarchyAbove<fr::BoundsComponent>(
				owningEntityRelationship.GetParent(),
				nextEntity);

			if (nextBounds != nullptr)
			{
				ExpandBoundsHierarchy(em, *this, nextEntity);
			}
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
	}


	void BoundsComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity owningEntity, bool startOpen /*= false*/)
	{
		const ImGuiTreeNodeFlags_ flags = startOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;

		if (ImGui::CollapsingHeader(
			std::format("Local bounds:##{}", static_cast<uint32_t>(owningEntity)).c_str(), flags))
		{
			ImGui::Indent();

			// RenderDataComponent:
			fr::RenderDataComponent::ShowImGuiWindow(em, owningEntity);

			fr::BoundsComponent const& boundsCmpt = em.GetComponent<fr::BoundsComponent>(owningEntity);

			ImGui::Text("Min XYZ = %s", glm::to_string(boundsCmpt.m_minXYZ).c_str());
			ImGui::Text("Max XYZ = %s", glm::to_string(boundsCmpt.m_maxXYZ).c_str());

			ImGui::Unindent();
		}
	}
}