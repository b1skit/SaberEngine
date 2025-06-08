// © 2023 Adam Badke. All rights reserved.
#include "Private/EntityManager.h"
#include "Private/RenderDataComponent.h"

#include "Renderer/RenderManager.h"
#include "Renderer/RenderSystem.h"


namespace fr
{
	std::atomic<gr::RenderDataID> RenderDataComponent::s_objectIDs = 0;


	RenderDataComponent* RenderDataComponent::GetCreateRenderDataComponent(
		fr::EntityManager& em, entt::entity entity, gr::TransformID transformID)
	{
		RenderDataComponent* renderDataCmpt = em.TryGetComponent<fr::RenderDataComponent>(entity);

		SEAssert(!renderDataCmpt || renderDataCmpt->GetTransformID() == transformID,
			"RenderDataComponent already exists, but is associated with a different TransformID");

		if (!renderDataCmpt)
		{
			em.EmplaceComponent<fr::RenderDataComponent::NewRegistrationMarker>(entity);
			renderDataCmpt = em.EmplaceComponent<fr::RenderDataComponent>(entity, PrivateCTORTag{}, transformID);
		}
		
		return renderDataCmpt;
	}


	RenderDataComponent& RenderDataComponent::AttachSharedRenderDataComponent(
		fr::EntityManager& em, entt::entity entity, RenderDataComponent const& renderDataComponent)
	{
		em.EmplaceComponent<fr::RenderDataComponent::NewRegistrationMarker>(entity);
		return *em.EmplaceComponent<fr::RenderDataComponent>(
			entity, 
			PrivateCTORTag{},
			renderDataComponent.m_renderDataID, 
			renderDataComponent.m_transformID);
	}


	void RenderDataComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity owningEntity)
	{
		ImGui::Indent();

		fr::RenderDataComponent const& renderDataCmpt = em.GetComponent<fr::RenderDataComponent>(owningEntity);
		ImGui::Text(std::format("RenderDataID: {}, TransformID: {}",
			renderDataCmpt.GetRenderDataID(), renderDataCmpt.GetTransformID()).c_str());

		ImGui::Unindent();
	}


	void RenderDataComponent::ShowImGuiWindow(std::vector<fr::RenderDataComponent const*> const& renderDataComponents)
	{		
		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;
		constexpr int numCols = 2;
		if (ImGui::BeginTable("m_IDToRenderObjectMetadata", numCols, flags))
		{
			// Headers:				
			ImGui::TableSetupColumn("RenderObjectID");
			ImGui::TableSetupColumn("TransformID");
			ImGui::TableHeadersRow();

			for (size_t i = 0; i < renderDataComponents.size(); i++)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				// RenderDataID
				ImGui::Text(std::format("{}", renderDataComponents[i]->GetRenderDataID()).c_str());

				ImGui::TableNextColumn();

				// TransformID
				ImGui::Text(std::format("{}", renderDataComponents[i]->GetTransformID()).c_str());
			}

			ImGui::EndTable();
		}
	}


	RenderDataComponent::RenderDataComponent(PrivateCTORTag, gr::TransformID transformID)
		: m_renderDataID(s_objectIDs.fetch_add(1)) // Allocate a new RenderDataID
		, m_transformID(transformID)
		, m_featureBits(0)
	{
	}


	RenderDataComponent::RenderDataComponent(PrivateCTORTag, gr::RenderDataID renderObjectID, gr::TransformID transformID)
		: m_renderDataID(renderObjectID)
		, m_transformID(transformID)
		, m_featureBits(0)
	{
	}


	RenderDataComponent::RenderDataComponent(PrivateCTORTag, RenderDataComponent const& sharedRenderDataComponent)
		: m_renderDataID(sharedRenderDataComponent.m_renderDataID) // Shared RenderDataID
		, m_transformID(sharedRenderDataComponent.m_transformID)
		, m_featureBits(0)
	{
	}


	gr::RenderDataID RenderDataComponent::GetRenderDataID() const
	{
		return m_renderDataID;
	}


	gr::TransformID RenderDataComponent::GetTransformID() const
	{
		return m_transformID;
	}


	void RenderDataComponent::SetFeatureBit(gr::RenderObjectFeature feature)
	{
		SEAssert(feature != gr::RenderObjectFeature::Invalid, "Invalid feature");
		m_featureBits.fetch_or(feature);
	}


	bool RenderDataComponent::HasFeatureBit(gr::RenderObjectFeature feature) const
	{
		SEAssert(feature != gr::RenderObjectFeature::Invalid, "Invalid feature");
		return (m_featureBits.load() & feature);
	}


	gr::FeatureBitmask RenderDataComponent::GetFeatureBits() const
	{
		return m_featureBits.load();
	}


	// ---


	RegisterRenderObjectCommand::RegisterRenderObjectCommand(RenderDataComponent const& newRenderDataComponent)
		: m_renderDataID(newRenderDataComponent.GetRenderDataID())
		, m_transformID(newRenderDataComponent.GetTransformID())
		, m_featureBits(newRenderDataComponent.GetFeatureBits())
	{
	}


	void RegisterRenderObjectCommand::Execute(void* cmdData)
	{
		RegisterRenderObjectCommand* cmdPtr = reinterpret_cast<RegisterRenderObjectCommand*>(cmdData);

		gr::RenderDataManager& renderData = re::RenderManager::Get()->GetRenderDataManagerForModification();

		renderData.RegisterObject(cmdPtr->m_renderDataID, cmdPtr->m_transformID);
		renderData.SetFeatureBits(cmdPtr->m_renderDataID, cmdPtr->m_featureBits);
	}


	void RegisterRenderObjectCommand::Destroy(void* cmdData)
	{
		RegisterRenderObjectCommand* cmdPtr = reinterpret_cast<RegisterRenderObjectCommand*>(cmdData);
		cmdPtr->~RegisterRenderObjectCommand();
	}


	// ---


	DestroyRenderObjectCommand::DestroyRenderObjectCommand(gr::RenderDataID objectID)
		: m_renderDataID(objectID)
	{
	}


	void DestroyRenderObjectCommand::Execute(void* cmdData)
	{
		DestroyRenderObjectCommand* cmdPtr = reinterpret_cast<DestroyRenderObjectCommand*>(cmdData);

		gr::RenderDataManager& renderData = re::RenderManager::Get()->GetRenderDataManagerForModification();

		renderData.DestroyObject(cmdPtr->m_renderDataID);
	}


	void DestroyRenderObjectCommand::Destroy(void* cmdData)
	{
		DestroyRenderObjectCommand* cmdPtr = reinterpret_cast<DestroyRenderObjectCommand*>(cmdData);
		cmdPtr->~DestroyRenderObjectCommand();
	}


	// ---


	RenderDataFeatureBitsRenderCommand::RenderDataFeatureBitsRenderCommand(
		gr::RenderDataID renderDataID, gr::FeatureBitmask featureBits)
		: m_renderDataID(renderDataID)
		, m_featureBits(featureBits)
	{
	}


	void RenderDataFeatureBitsRenderCommand::Execute(void* cmdData)
	{
		RenderDataFeatureBitsRenderCommand* cmdPtr = reinterpret_cast<RenderDataFeatureBitsRenderCommand*>(cmdData);

		gr::RenderDataManager& renderData = re::RenderManager::Get()->GetRenderDataManagerForModification();

		renderData.SetFeatureBits(cmdPtr->m_renderDataID, cmdPtr->m_featureBits);
	}


	void RenderDataFeatureBitsRenderCommand::Destroy(void* cmdData)
	{
		RenderDataFeatureBitsRenderCommand* cmdPtr = reinterpret_cast<RenderDataFeatureBitsRenderCommand*>(cmdData);
		cmdPtr->~RenderDataFeatureBitsRenderCommand();
	}
}