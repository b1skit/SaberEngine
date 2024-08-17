// © 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "EntityManager.h"
#include "MaterialInstanceComponent.h"
#include "MeshConcept.h"
#include "MeshPrimitiveComponent.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "SceneManager.h"
#include "SceneNodeConcept.h"
#include "TransformComponent.h"

#include "Core/Util/ImGuiUtils.h"

#include "Renderer/MeshFactory.h"


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
			Cone,
			Cylinder,
			HelloTriangle,

			MeshFactoryType_Count
		};
		constexpr std::array<char const*, MeshFactoryType::MeshFactoryType_Count> k_meshFactoryTypeNames =
		{
			"Quad",
			"Cube",					
			"Sphere",
			"Cone",
			"Cylinder",
			"Hello Triangle",
		};
		static_assert(k_meshFactoryTypeNames.size() == MeshFactoryType::MeshFactoryType_Count);

		constexpr ImGuiComboFlags k_comboFlags = 0;

		static SourceType s_selectedSrcType = static_cast<SourceType>(0);
		util::ShowBasicComboBox("Mesh source", k_sourceTypeNames.data(), k_sourceTypeNames.size(), s_selectedSrcType);	

		ImGui::Separator();

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
			uint32_t m_numLatSlices = 32;
			uint32_t m_numLongSlices = 32;
			std::array<char, k_nameInputBufferSize> m_nameInputBuffer = { "Spawned sphere\0" };
		};
		static SphereSpawnParams s_sphereSpawnParams;

		struct ConeSpawnParams
		{
			float m_height = 1.f;
			float m_radius = 0.5f; // Unit diameter
			uint32_t m_numSides = 64;
			gr::meshfactory::Orientation m_orientation;
			std::array<char, k_nameInputBufferSize> m_nameInputBuffer = { "Spawned cone\0" };
		};
		static ConeSpawnParams s_coneSpawnParams;

		struct CylinderSpawnParams
		{
			float m_height = 1.f;
			float m_radius = 0.5f; // Unit diameter
			uint32_t m_numSides = 24;
			std::array<char, k_nameInputBufferSize> m_nameInputBuffer = { "Spawned cylinder\0" };
		};
		static CylinderSpawnParams s_cylinderSpawnParams;

		struct HelloTriangleSpawnParams
		{
			float m_scale = 1.f;
			std::array<char, k_nameInputBufferSize> m_nameInputBuffer = { "Spawned hello triangle\0" };
		};
		static HelloTriangleSpawnParams s_helloTriangleSpawnParams;


		static char* s_nameInputBuffer = nullptr;
		static std::string s_meshFactoryMaterialName;

		static MeshFactoryType s_selectedFactoryType = static_cast<MeshFactoryType>(0);
		switch (s_selectedSrcType)
		{
		case SourceType::MeshFactory:
		{
			util::ShowBasicComboBox(
				"Factory type", k_meshFactoryTypeNames.data(), k_meshFactoryTypeNames.size(), s_selectedFactoryType);

			// Display any additional options needed for mesh factory construction:
			switch (s_selectedFactoryType)
			{
			case MeshFactoryType::Quad:
			{
				if (ImGui::InputFloat("Extent distance##quad", &s_quadSpawnParams.m_extentDistance))
				{
					s_quadSpawnParams.m_extentDistance = std::abs(s_quadSpawnParams.m_extentDistance);
				}

				s_nameInputBuffer = s_quadSpawnParams.m_nameInputBuffer.data();
			}
			break;
			case MeshFactoryType::Cube:
			{
				if (ImGui::InputFloat("Extent distance##cube", &s_cubeSpawnParams.m_extentDistance))
				{
					s_cubeSpawnParams.m_extentDistance = std::abs(s_cubeSpawnParams.m_extentDistance);
				}

				s_nameInputBuffer = s_cubeSpawnParams.m_nameInputBuffer.data();
			}
			break;
			case MeshFactoryType::Sphere:
			{
				if (ImGui::InputFloat("Radius##sphere", &s_sphereSpawnParams.m_radius))
				{
					s_sphereSpawnParams.m_radius = std::abs(s_sphereSpawnParams.m_radius);
				}
				ImGui::InputScalar(
					"Latitude slices#sphere", ImGuiDataType_::ImGuiDataType_U32, &s_sphereSpawnParams.m_numLatSlices);
				ImGui::InputScalar(
					"Longitude slices#sphere", ImGuiDataType_::ImGuiDataType_U32, &s_sphereSpawnParams.m_numLongSlices);
				
				s_nameInputBuffer = s_sphereSpawnParams.m_nameInputBuffer.data();
			}
			break;
			case MeshFactoryType::Cone:
			{
				if (ImGui::InputFloat("Height##cone", &s_coneSpawnParams.m_height))
				{
					s_coneSpawnParams.m_height = std::abs(s_coneSpawnParams.m_height);
				}
				if (ImGui::InputFloat("Radius##cone", &s_coneSpawnParams.m_radius))
				{
					s_coneSpawnParams.m_radius = std::abs(s_coneSpawnParams.m_radius);
				}
				ImGui::InputScalar(
					"Number of sides##cone", ImGuiDataType_::ImGuiDataType_U32, &s_coneSpawnParams.m_numSides);

				util::ShowBasicComboBox("Orientation",
					gr::meshfactory::k_orientationNames.data(), 
					gr::meshfactory::k_orientationNames.size(),
					s_coneSpawnParams.m_orientation);

				s_nameInputBuffer = s_coneSpawnParams.m_nameInputBuffer.data();
			}
			break;
			case MeshFactoryType::Cylinder:
			{
				if (ImGui::InputFloat("Height##cylinder", &s_cylinderSpawnParams.m_height))
				{
					s_cylinderSpawnParams.m_height = std::abs(s_cylinderSpawnParams.m_height);
				}
				if (ImGui::InputFloat("Radius##cylinder", &s_cylinderSpawnParams.m_radius))
				{
					s_cylinderSpawnParams.m_radius = std::abs(s_cylinderSpawnParams.m_radius);
				}
				ImGui::InputScalar(
					"Number of sides##cylinder", ImGuiDataType_::ImGuiDataType_U32, &s_cylinderSpawnParams.m_numSides);

				s_nameInputBuffer = s_cylinderSpawnParams.m_nameInputBuffer.data();
			}
			break;
			case MeshFactoryType::HelloTriangle:
			{
				ImGui::SliderFloat("Scale##hellotriangle", &s_helloTriangleSpawnParams.m_scale, 0.001f, 10.f);

				s_nameInputBuffer = s_helloTriangleSpawnParams.m_nameInputBuffer.data();
			}
			break;
			default: SEAssertF("Invalid mesh factory type");
			}


			// Material:
			static uint32_t s_selectedMaterialIdx = 0;
			std::vector<std::string> const& materialNames = re::RenderManager::GetSceneData()->GetAllMaterialNames();			
			
			util::ShowBasicComboBox(
				"Material##spawnMeshFactory", materialNames.data(), materialNames.size(), s_selectedMaterialIdx);

			s_meshFactoryMaterialName = materialNames[s_selectedMaterialIdx];

			// Name:
			ImGui::InputText("Object name", s_nameInputBuffer, k_nameInputBufferSize);
		}
		break;
		case SourceType::GLTFFile:
		{
			ImGui::TextDisabled("TODO");
		}
		break;
		default: SEAssertF("Invalid selected source type");
		}

		ImGui::Separator();

		// Spawn!
		if (ImGui::Button("Spawn"))
		{
			switch (s_selectedSrcType)
			{
			case SourceType::MeshFactory:
			{
				entt::entity sceneNode = fr::SceneNode::Create(*fr::EntityManager::Get(), s_nameInputBuffer, entt::null);
				fr::Mesh::AttachMeshConcept(sceneNode, s_nameInputBuffer);

				const gr::meshfactory::FactoryOptions factoryOptions
				{
					.m_generateNormalsAndTangents = true,
					.m_vertexColor = glm::vec4(1.f) // GLTF default
				};

				std::shared_ptr<gr::MeshPrimitive> mesh = nullptr;
				switch (s_selectedFactoryType)
				{
				case MeshFactoryType::Quad:
				{
					mesh = gr::meshfactory::CreateQuad(factoryOptions, s_quadSpawnParams.m_extentDistance);
				}
				break;
				case MeshFactoryType::Cube:
				{
					mesh = gr::meshfactory::CreateCube(factoryOptions, s_cubeSpawnParams.m_extentDistance);
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
				case MeshFactoryType::Cone:
				{
					gr::meshfactory::FactoryOptions coneFactoryOptions = factoryOptions;
					coneFactoryOptions.m_orientation = s_coneSpawnParams.m_orientation;

					mesh = gr::meshfactory::CreateCone(
						coneFactoryOptions,
						s_coneSpawnParams.m_height,
						s_coneSpawnParams.m_radius,
						s_coneSpawnParams.m_numSides);
				}
				break;
				case MeshFactoryType::Cylinder:
				{
					mesh = gr::meshfactory::CreateCylinder(
						factoryOptions,
						s_cylinderSpawnParams.m_height,
						s_cylinderSpawnParams.m_radius,
						s_cylinderSpawnParams.m_numSides);
				}
				break;
				case MeshFactoryType::HelloTriangle:
				{
					mesh = gr::meshfactory::CreateHelloTriangle(factoryOptions, s_helloTriangleSpawnParams.m_scale, 0.f);
				}
				break;
				default: SEAssertF("Invalid mesh factory type");
				}

				entt::entity meshPrimimitiveEntity = fr::MeshPrimitiveComponent::CreateMeshPrimitiveConcept(
					*fr::EntityManager::Get(),
					sceneNode,
					mesh.get(),
					fr::BoundsComponent::k_invalidMinXYZ,
					fr::BoundsComponent::k_invalidMaxXYZ);

				// Attach a material:
				std::shared_ptr<gr::Material> material =
					re::RenderManager::GetSceneData()->GetMaterial(s_meshFactoryMaterialName.c_str());

				fr::MaterialInstanceComponent::AttachMaterialComponent(
					*fr::EntityManager::Get(), meshPrimimitiveEntity, material.get());
			}
			break;
			case SourceType::GLTFFile:
			{
				ImGui::TextDisabled("TODO");
			}
			break;
			default: SEAssertF("Invalid selected source type");
			}
		}
	}
}