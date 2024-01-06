// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "RenderDataComponent.h"
#include "RenderManager.h"
#include "RenderSystem.h"


namespace gr
{
	std::atomic<gr::RenderDataID> RenderDataComponent::s_objectIDs = 0;


	RenderDataComponent& RenderDataComponent::AttachNewRenderDataComponent(
		fr::EntityManager& em, entt::entity entity, TransformID transformID)
	{
		em.EmplaceComponent<gr::RenderDataComponent::NewRegistrationMarker>(entity);
		return *em.EmplaceComponent<gr::RenderDataComponent>(entity, PrivateCTORTag{}, transformID);
	}


	RenderDataComponent& RenderDataComponent::AttachSharedRenderDataComponent(
		fr::EntityManager& em, entt::entity entity, RenderDataComponent const& renderDataComponent)
	{
		em.EmplaceComponent<gr::RenderDataComponent::NewRegistrationMarker>(entity);
		return *em.EmplaceComponent<gr::RenderDataComponent>(
			entity, 
			PrivateCTORTag{},
			renderDataComponent.m_renderDataID, 
			renderDataComponent.m_transformID);
	}


	void RenderDataComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity owningEntity)
	{
		ImGui::Indent();

		gr::RenderDataComponent const& renderDataCmpt = em.GetComponent<gr::RenderDataComponent>(owningEntity);
		ImGui::Text(std::format("RenderDataID: {}, TransformID: {}",
			renderDataCmpt.GetRenderDataID(), renderDataCmpt.GetTransformID()).c_str());

		ImGui::Unindent();
	}


	void RenderDataComponent::ShowImGuiWindow(std::vector<gr::RenderDataComponent const*> const& renderDataComponents)
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
		SEAssert("Invalid feature", feature != gr::RenderObjectFeature::Invalid);
		m_featureBits |= (1 << feature);
	}


	gr::FeatureBitmask RenderDataComponent::GetFeatureBits() const
	{
		return m_featureBits;
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
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		RegisterRenderObjectCommand* cmdPtr = reinterpret_cast<RegisterRenderObjectCommand*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			gr::RenderDataManager& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();

			renderData.RegisterObject(cmdPtr->m_renderDataID, cmdPtr->m_transformID);
			renderData.SetFeatureBits(cmdPtr->m_renderDataID, cmdPtr->m_featureBits);
		}
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
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		DestroyRenderObjectCommand* cmdPtr = reinterpret_cast<DestroyRenderObjectCommand*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			gr::RenderDataManager& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();

			renderData.DestroyObject(cmdPtr->m_renderDataID);
		}
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
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		RenderDataFeatureBitsRenderCommand* cmdPtr = reinterpret_cast<RenderDataFeatureBitsRenderCommand*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			gr::RenderDataManager& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();

			renderData.SetFeatureBits(cmdPtr->m_renderDataID, cmdPtr->m_featureBits);
		}
	}


	void RenderDataFeatureBitsRenderCommand::Destroy(void* cmdData)
	{
		RenderDataFeatureBitsRenderCommand* cmdPtr = reinterpret_cast<RenderDataFeatureBitsRenderCommand*>(cmdData);
		cmdPtr->~RenderDataFeatureBitsRenderCommand();
	}
}