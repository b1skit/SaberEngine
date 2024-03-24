// © 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "EntityManager.h"
#include "ImGuiUtils.h"
#include "MaterialInstanceComponent.h"
#include "MeshConcept.h"
#include "MeshFactory.h"
#include "MeshPrimitiveComponent.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "SceneManager.h"
#include "SceneNodeConcept.h"
#include "TransformComponent.h"


namespace fr
{
	void Mesh::AttachMeshConcept(entt::entity owningEntity, char const* name)
	{
		fr::EntityManager& em = *fr::EntityManager::Get();

		SEAssert(em.HasComponent<fr::TransformComponent>(owningEntity),
			"A Mesh concept requires a Transform. The owningEntity should have this already");
		
		em.EmplaceComponent<fr::Mesh::MeshConceptMarker>(owningEntity);

		fr::TransformComponent const& owningTransformCmpt = em.GetComponent<fr::TransformComponent>(owningEntity);

		gr::RenderDataComponent& meshRenderData = 
			gr::RenderDataComponent::AttachNewRenderDataComponent(em, owningEntity, owningTransformCmpt.GetTransformID());

		// Mesh bounds: Encompasses all attached primitive bounds
		fr::BoundsComponent::AttachBoundsComponent(em, owningEntity);

		// Mark our RenderDataComponent so the renderer can differentiate between Mesh and MeshPrimitive Bounds
		meshRenderData.SetFeatureBit(gr::RenderObjectFeature::IsMeshBounds);
	}


	void Mesh::ShowImGuiWindow(fr::EntityManager& em, entt::entity meshConcept)
	{
		fr::NameComponent const& meshName = em.GetComponent<fr::NameComponent>(meshConcept);

		if (ImGui::CollapsingHeader(
			std::format("Mesh \"{}\"##{}", meshName.GetName(), meshName.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			gr::RenderDataComponent::ShowImGuiWindow(em, meshConcept);

			fr::Relationship const& meshRelationship = em.GetComponent<fr::Relationship>(meshConcept);

			fr::TransformComponent& owningTransform = em.GetComponent<fr::TransformComponent>(meshConcept);

			// Transform:
			fr::TransformComponent::TransformComponent::ShowImGuiWindow(
				em, meshConcept, static_cast<uint64_t>(meshConcept));

			// Bounds:
			fr::BoundsComponent::ShowImGuiWindow(em, meshConcept);

			// Mesh primitives:
			if (ImGui::CollapsingHeader(
				std::format("Mesh Primitives:##{}", meshName.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();

				entt::entity curChild = meshRelationship.GetFirstChild();
				do
				{
					fr::MeshPrimitiveComponent& meshPrimCmpt = em.GetComponent<fr::MeshPrimitiveComponent>(curChild);

					meshPrimCmpt.ShowImGuiWindow(em, curChild);

					fr::Relationship const& childRelationship = em.GetComponent<fr::Relationship>(curChild);
					curChild = childRelationship.GetNext();
				} while (curChild != meshRelationship.GetFirstChild());

				ImGui::Unindent();
			}

			ImGui::Unindent();
		}
	}


	void Mesh::ShowImGuiSpawnWindow()
	{
		enum SourceType : uint8_t
		{
			MeshFactory,
			GLTFFile,

			SourceType_Count
		};
		constexpr std::array<char const*, SourceType::SourceType_Count> k_sourceTypeNames =
		{
			"Mesh Factory",
			"GLTF File"
		};
		static_assert(k_sourceTypeNames.size() == SourceType::SourceType_Count);

		enum MeshFactoryType : uint8_t
		{
			Quad,
			Cube,
			Sphere,
			HelloTriangle,

			MeshFactoryType_Count
		};
		constexpr std::array<char const*, MeshFactoryType::MeshFactoryType_Count> k_meshFactoryTypeNames =
		{
			"Quad",
			"Cube",					
			"Sphere",
			"Hello Triangle"
		};
		static_assert(k_meshFactoryTypeNames.size() == MeshFactoryType::MeshFactoryType_Count);

		constexpr ImGuiComboFlags k_comboFlags = 0;

		static SourceType s_selectedSrcType = static_cast<SourceType>(0);
		if (ImGui::BeginCombo("Mesh source", k_sourceTypeNames[s_selectedSrcType], k_comboFlags))
		{
			for (uint8_t comboIdx = 0; comboIdx < k_sourceTypeNames.size(); comboIdx++)
			{
				const bool isSelected = comboIdx == s_selectedSrcType;
				if (ImGui::Selectable(k_sourceTypeNames[comboIdx], isSelected))
				{
					s_selectedSrcType = static_cast<SourceType>(comboIdx);
				}

				// Set the initial focus:
				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		constexpr size_t k_nameInputBufferSize = 128;
		struct CubeSpawnParams
		{
			float m_extentDistance = 0.5f; // Unit width/height/depth
			std::array<char, k_nameInputBufferSize> m_nameInputBuffer = { "Spawned cube\0" };
		};
		static CubeSpawnParams s_cubeSpawnParams;

		struct QuadSpawnParams
		{
			float m_extentDistance = 0.5f; // Unit width/height
			std::array<char, k_nameInputBufferSize> m_nameInputBuffer = { "Spawned quad\0" };
		};
		static QuadSpawnParams s_quadSpawnParams;

		struct SphereSpawnParams
		{
			float m_radius = 0.5f; // Unit diameter
			uint32_t m_numLatSlices = 16;
			uint32_t m_numLongSlices = 16;
			std::array<char, k_nameInputBufferSize> m_nameInputBuffer = { "Spawned sphere\0" };
		};
		static SphereSpawnParams s_sphereSpawnParams;

		struct HelloTriangleSpawnParams
		{
			float m_scale = 1.f;
			std::array<char, k_nameInputBufferSize> m_nameInputBuffer = { "Spawned hello triangle\0" };
		};
		static HelloTriangleSpawnParams s_helloTriangleSpawnParams;

		static char* s_nameInputArr = nullptr;

		static MeshFactoryType s_selectedFactoryType = static_cast<MeshFactoryType>(0);
		switch (s_selectedSrcType)
		{
		case SourceType::MeshFactory:
		{
			if (ImGui::BeginCombo("Factory type", k_meshFactoryTypeNames[s_selectedFactoryType], k_comboFlags))
			{
				for (uint8_t comboIdx = 0; comboIdx < k_meshFactoryTypeNames.size(); comboIdx++)
				{
					const bool isSelected = comboIdx == s_selectedFactoryType;
					if (ImGui::Selectable(k_meshFactoryTypeNames[comboIdx], isSelected))
					{
						s_selectedFactoryType = static_cast<MeshFactoryType>(comboIdx);
					}

					if (isSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			// Display any additional options needed for mesh factory construction:
			switch (s_selectedFactoryType)
			{
			case MeshFactoryType::Quad:
			{
				if (ImGui::InputFloat("Extent distance##quad", &s_quadSpawnParams.m_extentDistance))
				{
					s_quadSpawnParams.m_extentDistance = std::abs(s_quadSpawnParams.m_extentDistance);
				}

				s_nameInputArr = s_quadSpawnParams.m_nameInputBuffer.data();
			}
			break;
			case MeshFactoryType::Cube:
			{
				if (ImGui::InputFloat("Extent distance##cube", &s_cubeSpawnParams.m_extentDistance))
				{
					s_cubeSpawnParams.m_extentDistance = std::abs(s_cubeSpawnParams.m_extentDistance);
				}

				s_nameInputArr = s_cubeSpawnParams.m_nameInputBuffer.data();
			}
			break;
			case MeshFactoryType::Sphere:
			{
				if (ImGui::InputFloat("Radius##sphere", &s_sphereSpawnParams.m_radius))
				{
					s_sphereSpawnParams.m_radius = std::abs(s_sphereSpawnParams.m_radius);
				}
				ImGui::InputScalar(
					"Latitude slices", ImGuiDataType_::ImGuiDataType_U32, &s_sphereSpawnParams.m_numLatSlices);
				ImGui::InputScalar(
					"Longitude slices", ImGuiDataType_::ImGuiDataType_U32, &s_sphereSpawnParams.m_numLongSlices);
				
				s_nameInputArr = s_sphereSpawnParams.m_nameInputBuffer.data();
			}
			break;
			case MeshFactoryType::HelloTriangle:
			{
				ImGui::SliderFloat("Scale##hellotriangle", &s_helloTriangleSpawnParams.m_scale, 0.001f, 10.f);

				s_nameInputArr = s_helloTriangleSpawnParams.m_nameInputBuffer.data();
			}
			break;
			default: SEAssertF("Invalid mesh factory type");
			}
		}
		break;
		case SourceType::GLTFFile:
		{
			ImGui::TextDisabled("TODO");
		}
		break;
		default: SEAssertF("Invalid selected source type");
		}
		
		ImGui::InputText("Name", s_nameInputArr, k_nameInputBufferSize);

		if (ImGui::Button("Spawn"))
		{
			std::shared_ptr<gr::MeshPrimitive> mesh = nullptr;

			entt::entity sceneNode = fr::SceneNode::Create(*fr::EntityManager::Get(), s_nameInputArr, entt::null);

			fr::Mesh::AttachMeshConcept(sceneNode, s_nameInputArr);

			entt::entity meshPrimimitiveEntity = entt::null;

			const gr::meshfactory::FactoryOptions factoryOptions
			{
				.m_generateNormalsAndTangents = true,
				.m_generateVertexColors = true,
				.m_vertexColor = glm::vec4(1.f)
			};

			switch (s_selectedSrcType)
			{
			case SourceType::MeshFactory:
			{
				switch (s_selectedFactoryType)
				{
				case MeshFactoryType::Quad:
				{
					mesh = gr::meshfactory::CreateQuad(factoryOptions, s_quadSpawnParams.m_extentDistance);
				}
				break;
				case MeshFactoryType::Cube:
				{
					mesh = gr::meshfactory::CreateCube(s_cubeSpawnParams.m_extentDistance, factoryOptions);
				}
				break;
				case MeshFactoryType::Sphere:
				{
					mesh = gr::meshfactory::CreateSphere(
						factoryOptions,
						s_sphereSpawnParams.m_radius,
						s_sphereSpawnParams.m_numLatSlices,
						s_sphereSpawnParams.m_numLongSlices);
				}
				break;
				case MeshFactoryType::HelloTriangle:
				{
					mesh = gr::meshfactory::CreateHelloTriangle(factoryOptions, s_helloTriangleSpawnParams.m_scale);
				}
				break;
				default: SEAssertF("Invalid mesh factory type");
				}
			}
			break;
			case SourceType::GLTFFile:
			{
				ImGui::TextDisabled("TODO");
			}
			break;
			default: SEAssertF("Invalid selected source type");
			}

			meshPrimimitiveEntity = fr::MeshPrimitiveComponent::CreateMeshPrimitiveConcept(
				*fr::EntityManager::Get(),
				sceneNode,
				mesh.get(),
				fr::BoundsComponent::k_invalidMinXYZ,
				fr::BoundsComponent::k_invalidMaxXYZ);

			// Attach a material:
			std::shared_ptr<gr::Material> material = 
				fr::SceneManager::GetSceneData()->GetMaterial(fr::SceneData::k_missingMaterialName);
			
			fr::MaterialInstanceComponent::AttachMaterialComponent(
				*fr::EntityManager::Get(), meshPrimimitiveEntity, material);
		}
	}
}