// © 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "EntityManager.h"
#include "MaterialInstanceComponent.h"
#include "MeshConcept.h"
#include "MeshPrimitiveComponent.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "SceneNodeConcept.h"
#include "SkinningComponent.h"
#include "TransformComponent.h"

#include "Core/Inventory.h"

#include "Core/Definitions/ConfigKeys.h"

#include "Core/Util/ImGuiUtils.h"

#include "Renderer/MeshFactory.h"


namespace pr
{
	void Mesh::AttachMeshConceptMarker(pr::EntityManager& em, entt::entity owningEntity, char const* name)
	{	
		em.EmplaceComponent<pr::Mesh::MeshConceptMarker>(owningEntity);

		pr::Relationship const& relationship = em.GetComponent<pr::Relationship>(owningEntity);

		pr::TransformComponent const* transformCmpt = relationship.GetFirstInHierarchyAbove<pr::TransformComponent>();
		if (!transformCmpt)
		{
			transformCmpt = &pr::TransformComponent::AttachTransformComponent(em, owningEntity);
		}

		pr::RenderDataComponent* meshRenderData = 
			pr::RenderDataComponent::GetCreateRenderDataComponent(em, owningEntity, transformCmpt->GetTransformID());

		// Before we attach a BoundsComponent, search the hierarchy above for a potential encapsulation
		const entt::entity encapsulatingBounds =
			relationship.GetFirstEntityInHierarchyAbove<pr::Mesh::MeshConceptMarker, pr::BoundsComponent>();

		// Mesh bounds: Encompasses all attached primitive bounds
		pr::BoundsComponent::AttachBoundsComponent(em, owningEntity, encapsulatingBounds);

		// Mark our RenderDataComponent so the renderer can differentiate between Mesh and MeshPrimitive Bounds
		meshRenderData->SetFeatureBit(gr::RenderObjectFeature::IsMeshBounds);
	}


	void Mesh::ShowImGuiWindow(pr::EntityManager& em, entt::entity meshConcept)
	{
		pr::NameComponent const& meshName = em.GetComponent<pr::NameComponent>(meshConcept);

		if (ImGui::CollapsingHeader(
			std::format("Mesh \"{}\"##{}", meshName.GetName(), meshName.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			pr::RenderDataComponent::ShowImGuiWindow(em, meshConcept);

			pr::Relationship const& meshRelationship = em.GetComponent<pr::Relationship>(meshConcept);

			// Transform:
			ImGui::PushID(static_cast<uint64_t>(meshConcept));
			pr::TransformComponent::TransformComponent::ShowImGuiWindow(
				em, meshConcept, static_cast<uint64_t>(meshConcept));
			ImGui::PopID();

			// Bounds:
			pr::BoundsComponent::ShowImGuiWindow(em, meshConcept);

			// Mesh primitives:
			const uint32_t numMeshPrims = meshRelationship.GetNumInImmediateChildren<pr::MeshPrimitiveComponent>();
			if (ImGui::CollapsingHeader(
				std::format("Mesh Primitives ({})##{}", numMeshPrims, meshName.GetUniqueID()).c_str(),
				ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();

				entt::entity curChild = meshRelationship.GetFirstChild();
				do
				{
					pr::MeshPrimitiveComponent* meshPrimCmpt = em.TryGetComponent<pr::MeshPrimitiveComponent>(curChild);
					if (meshPrimCmpt)
					{
						meshPrimCmpt->ShowImGuiWindow(em, curChild);
					}

					pr::Relationship const& childRelationship = em.GetComponent<pr::Relationship>(curChild);
					curChild = childRelationship.GetNext();
				} while (curChild != meshRelationship.GetFirstChild());

				ImGui::Unindent();
			}

			// Skinning component:
			pr::SkinningComponent::ShowImGuiWindow(em, meshConcept);

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

		pr::EntityManager* em = pr::EntityManager::Get();

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
			std::vector<std::string> materialNames;
			{
				std::vector<entt::entity> const& materialEntities = em->GetAllEntities<pr::MaterialInstanceComponent>();

				materialNames.reserve(materialEntities.size() + 1); // +1 for the default material

				// Build a vector of unique material names (multiple material instances can share the same base material)
				std::unordered_set<std::string> seenMaterials;
				for (entt::entity matEntity : materialEntities)
				{
					pr::MaterialInstanceComponent& material = em->GetComponent<pr::MaterialInstanceComponent>(matEntity);

					if (!seenMaterials.contains(material.GetMaterial()->GetName()))
					{
						materialNames.emplace_back(material.GetMaterial()->GetName());
						seenMaterials.emplace(material.GetMaterial()->GetName());
					}
				}

				// Add the default GLTF material if we haven't seen it yet.
				// TODO: This is brittle, we shouldn't be specifically referencing any specific material model here, but
				// for now it prevents a crash when no materials exist
				if (!seenMaterials.contains(en::DefaultResourceNames::k_defaultGLTFMaterialName))
				{
					materialNames.emplace_back(en::DefaultResourceNames::k_defaultGLTFMaterialName);
					seenMaterials.emplace(en::DefaultResourceNames::k_defaultGLTFMaterialName);
				}				
			}
			
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
				const entt::entity sceneNode = pr::SceneNode::Create(*em, s_nameInputBuffer, entt::null);
				pr::Mesh::AttachMeshConceptMarker(*em, sceneNode, s_nameInputBuffer);

				glm::vec3 minXYZ = glm::vec3(0.f);
				glm::vec3 maxXYZ = glm::vec3(0.f);
				const gr::meshfactory::FactoryOptions factoryOptions
				{
					.m_generateNormalsAndTangents = true,
					.m_vertexColor = glm::vec4(1.f), // GLTF default
					.m_positionMinXYZOut = &minXYZ,
					.m_positionMaxXYZOut = &maxXYZ,
				};

				core::InvPtr<gr::MeshPrimitive> mesh;
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

				entt::entity meshPrimimitiveEntity = pr::MeshPrimitiveComponent::CreateMeshPrimitiveConcept(
					*em,
					sceneNode,
					mesh,
					minXYZ,
					maxXYZ);

				// Attach a material:
				core::InvPtr<gr::Material> const& material =
					core::Inventory::Get<gr::Material>(s_meshFactoryMaterialName.c_str());

				pr::MaterialInstanceComponent::AttachMaterialComponent(
					*em, meshPrimimitiveEntity, material);
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