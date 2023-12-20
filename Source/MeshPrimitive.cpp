// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "GameplayManager.h"
#include "MarkerComponents.h"
#include "MeshPrimitive.h"
#include "NameComponent.h"
#include "PipelineState.h"
#include "Relationship.h"
#include "RenderDataComponent.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "Transform.h"
#include "TransformComponent.h"

using en::Config;
using gr::Transform;
using fr::Bounds;
using gr::MeshPrimitive;
using glm::pi;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using std::move;
using std::vector;
using std::string;
using std::shared_ptr;


namespace
{
	constexpr char const* DrawModeToCStr(gr::MeshPrimitive::TopologyMode drawMode)
	{
		switch (drawMode)
		{
		case gr::MeshPrimitive::TopologyMode::PointList: return "PointList";
		case gr::MeshPrimitive::TopologyMode::LineList: return "LineList";
		case gr::MeshPrimitive::TopologyMode::LineStrip: return "LineStrip";
		case gr::MeshPrimitive::TopologyMode::TriangleList: return "TriangleList";
		case gr::MeshPrimitive::TopologyMode::TriangleStrip: return "TriangleStrip";
		case gr::MeshPrimitive::TopologyMode::LineListAdjacency: return "LineListAdjacency";
		case gr::MeshPrimitive::TopologyMode::LineStripAdjacency: return "LineStripAdjacency";
		case gr::MeshPrimitive::TopologyMode::TriangleListAdjacency: return "TriangleListAdjacency";
		case gr::MeshPrimitive::TopologyMode::TriangleStripAdjacency: return "TriangleStripAdjacency";
		default: SEAssertF("Invalid draw mode");
		}
		return "INVALID DRAW MODE";
	}


	constexpr char const* SlotToCStr(gr::MeshPrimitive::Slot slot)
	{
		switch (slot)
		{
		case gr::MeshPrimitive::Slot::Position: return "Position";
		case gr::MeshPrimitive::Slot::Normal: return "Normal";
		case gr::MeshPrimitive::Slot::Tangent: return "Tangent";
		case gr::MeshPrimitive::Slot::UV0: return "UV0";
		case gr::MeshPrimitive::Slot::Color: return "Color";
		case gr::MeshPrimitive::Slot::Joints: return "Joints";
		case gr::MeshPrimitive::Slot::Weights: return "Weights";
		default:SEAssertF("Invalid slot index");
		}
		return "INVALID SLOT INDEX";
	}
}

namespace gr
{
	MeshPrimitive::RenderData MeshPrimitive::GetRenderData(MeshPrimitiveComponent const& meshPrimitiveComponent)
	{
		MeshPrimitive::RenderData renderData = MeshPrimitive::RenderData{
			.m_meshPrimitiveParams = meshPrimitiveComponent.m_meshPrimitive->GetMeshParams(),
			// Vertex streams copied below...
			.m_indexStream = meshPrimitiveComponent.m_meshPrimitive->m_indexStream.get(),
			.m_dataHash = meshPrimitiveComponent.m_meshPrimitive->GetDataHash()
		};
		
		std::vector<re::VertexStream const*> const& vertexStreams = 
			meshPrimitiveComponent.m_meshPrimitive->GetVertexStreams();
		for (size_t slotIdx = 0; slotIdx < vertexStreams.size(); slotIdx++)
		{
			renderData.m_vertexStreams[slotIdx] = vertexStreams[slotIdx];
		}
		
		return renderData;
	}


	entt::entity MeshPrimitive::AttachMeshPrimitiveConcept(
		entt::entity meshConcept,
		char const* name,
		std::vector<uint32_t>* indices,
		std::vector<float>& positions,
		glm::vec3 const& positionMinXYZ, // Pass fr::BoundsConcept::k_invalidMinXYZ to compute bounds manually
		glm::vec3 const& positionMaxXYZ, // Pass fr::BoundsConcept::k_invalidMaxXYZ to compute bounds manually
		std::vector<float>* normals,
		std::vector<float>* tangents,
		std::vector<float>* uv0,
		std::vector<float>* colors,
		std::vector<uint8_t>* joints,
		std::vector<float>* weights,
		gr::MeshPrimitive::MeshPrimitiveParams const& meshParams)
	{
		fr::GameplayManager& gpm = *fr::GameplayManager::Get();

		SEAssert("A mesh primitive's owning mesh concept requires a Relationship component",
			gpm.HasComponent<fr::Relationship>(meshConcept));
		SEAssert("A mesh primitive's owning mesh concept requires a TransformComponent",
			fr::Relationship::HasComponentInParentHierarchy<fr::TransformComponent>(meshConcept));
		SEAssert("A mesh primitive's owning mesh concept requires a Bounds component",
			gpm.HasComponent<fr::Bounds>(meshConcept));
		SEAssert("A mesh primitive's owning mesh concept requires a NameComponent",
			gpm.HasComponent<fr::NameComponent>(meshConcept));

		entt::entity meshPrimitiveEntity = gpm.CreateEntity(name);

		// Each MeshPrimitive is an entity related to a Mesh concept via a Relationship:
		fr::Relationship& meshRelationship = fr::Relationship::AttachRelationshipComponent(gpm, meshPrimitiveEntity);
		meshRelationship.SetParent(gpm, meshConcept);

		// Attach a new RenderDataComponent:
		fr::TransformComponent const* transformComponent = 
			fr::Relationship::GetComponentInHierarchyAbove<fr::TransformComponent>(meshConcept);

		gr::RenderDataComponent::AttachNewRenderDataComponent(
			gpm, meshPrimitiveEntity, transformComponent->GetTransformID());


		std::shared_ptr<gr::MeshPrimitive> meshPrimitiveSceneData = Create(
			name,
			indices,
			positions,
			positionMinXYZ, // ECS_CONVERSION TODO: Deprecated!!!!!!!!!!!!!!!!!!!!!
			positionMaxXYZ, // ECS_CONVERSION TODO: Deprecated!!!!!!!!!!!!!!!!!!!!!
			normals,
			tangents,
			uv0,
			colors,
			joints,
			weights,
			nullptr, // ECS_CONVERSION TODO: Deprecated!!!!!!!!!!!!!!!!!!!!!
			meshParams);

		// Attach a MeshPrimitive object:
		gpm.EmplaceComponent<gr::MeshPrimitive::MeshPrimitiveComponent>(
			meshPrimitiveEntity, 
			MeshPrimitiveComponent{
				.m_meshPrimitive = meshPrimitiveSceneData.get()
			});

		// Attach a primitive bounds
		fr::Bounds::AttachBoundsComponent(
			gpm, 
			meshPrimitiveEntity, 
			positionMinXYZ, 
			positionMaxXYZ, 
			reinterpret_cast<std::vector<glm::vec3> const&>(positions));
		fr::Bounds const& meshPrimitiveBounds = gpm.GetComponent<fr::Bounds>(meshPrimitiveEntity);

		// Update the Bounds of the MeshConcept:
		fr::Bounds& meshConceptBounds = gpm.GetComponent<fr::Bounds>(meshConcept);
		meshConceptBounds.ExpandBounds(meshPrimitiveBounds);

		// Mark our new MeshPrimitive as dirty:
		gpm.EmplaceComponent<DirtyMarker<gr::MeshPrimitive::MeshPrimitiveComponent>>(meshPrimitiveEntity);

		// Note: A Material component must be attached to the returned entity
		return meshPrimitiveEntity;
	}


	std::shared_ptr<MeshPrimitive> MeshPrimitive::Create(
		std::string const& name,
		std::vector<uint32_t>* indices,
		std::vector<float>& positions,
		glm::vec3 const& positionMinXYZ, // Pass fr::Bounds::k_invalidMinXYZ to compute bounds manually
		glm::vec3 const& positionMaxXYZ, // Pass fr::Bounds::k_invalidMaxXYZ to compute bounds manually
		std::vector<float>* normals,
		std::vector<float>* tangents,
		std::vector<float>* uv0,
		std::vector<float>* colors,
		std::vector<uint8_t>* joints,
		std::vector<float>* weights,
		std::shared_ptr<gr::Material> material,
		gr::MeshPrimitive::MeshPrimitiveParams const& meshParams)
	{
		shared_ptr<MeshPrimitive> newMeshPrimitive;
		newMeshPrimitive.reset(new MeshPrimitive(
			name.c_str(),
			indices,
			positions,
			positionMinXYZ,
			positionMaxXYZ,
			normals,
			tangents,
			uv0,
			colors,
			joints,
			weights,
			material,
			meshParams));

		// This call will replace the newMeshPrimitive pointer if a duplicate MeshPrimitive already exists
		en::SceneManager::GetSceneData()->AddUniqueMeshPrimitive(newMeshPrimitive);

		return newMeshPrimitive;
	}


	MeshPrimitive::MeshPrimitive(
		char const* name,
		vector<uint32_t>* indices,
		vector<float>& positions,
		glm::vec3 const& positionMinXYZ,
		glm::vec3 const& positionMaxXYZ,
		vector<float>* normals,
		vector<float>* tangents,
		vector<float>* uv0,
		vector<float>* colors,
		vector<uint8_t>* joints,
		vector<float>* weights,
		shared_ptr<gr::Material> material,
		MeshPrimitiveParams const& meshParams)
		: NamedObject(name)
		, m_meshMaterial(material)
		, m_params(meshParams)
		, m_localBounds(positionMinXYZ, positionMaxXYZ, reinterpret_cast<std::vector<glm::vec3> const&>(positions))
	{
		m_indexStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::Permanent,
			re::VertexStream::StreamType::Index,
			1, 
			re::VertexStream::DataType::UInt,
			re::VertexStream::Normalize::False,
			std::move(*indices));

		m_vertexStreams[Slot::Position] = re::VertexStream::Create(
			re::VertexStream::Lifetime::Permanent,
			re::VertexStream::StreamType::Vertex,
			3,
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(positions));

		if (normals && !normals->empty())
		{
			m_vertexStreams[Slot::Normal] = re::VertexStream::Create(
				re::VertexStream::Lifetime::Permanent,
				re::VertexStream::StreamType::Vertex,
				3,
				re::VertexStream::DataType::Float,
				re::VertexStream::Normalize::True,
				std::move(*normals));
		}

		if (colors && !colors->empty())
		{
			m_vertexStreams[Slot::Color] = re::VertexStream::Create(
				re::VertexStream::Lifetime::Permanent,
				re::VertexStream::StreamType::Vertex,
				4,
				re::VertexStream::DataType::Float,
				re::VertexStream::Normalize::False,
				std::move(*colors));
		}

		if (uv0 && !uv0->empty())
		{
			m_vertexStreams[Slot::UV0] = re::VertexStream::Create(
				re::VertexStream::Lifetime::Permanent,
				re::VertexStream::StreamType::Vertex,
				2,
				re::VertexStream::DataType::Float,
				re::VertexStream::Normalize::False,
				std::move(*uv0));
		}

		if (tangents && !tangents->empty())
		{
			m_vertexStreams[Slot::Tangent] = re::VertexStream::Create(
				re::VertexStream::Lifetime::Permanent,
				re::VertexStream::StreamType::Vertex,
				4,
				re::VertexStream::DataType::Float,
				re::VertexStream::Normalize::True,
				std::move(*tangents));
		}
		
		if (joints && !joints->empty())
		{
			m_vertexStreams[Slot::Joints] = re::VertexStream::Create(
				re::VertexStream::Lifetime::Permanent,
				re::VertexStream::StreamType::Vertex,
				1,
				re::VertexStream::DataType::UByte,
				re::VertexStream::Normalize::False,
				std::move(*joints));
		}

		if (weights && !weights->empty())
		{
			m_vertexStreams[Slot::Weights] = re::VertexStream::Create(
				re::VertexStream::Lifetime::Permanent,
				re::VertexStream::StreamType::Vertex,
				1,
				re::VertexStream::DataType::Float,
				re::VertexStream::Normalize::False,
				std::move(*weights));
		}

		ComputeDataHash();
	}


	void MeshPrimitive::Destroy()
	{
		for (uint8_t slotIdx = 0; slotIdx < Slot_Count; slotIdx++)
		{
			m_vertexStreams[slotIdx] = nullptr;
		}
		
		m_meshMaterial = nullptr;
	}


	void MeshPrimitive::ComputeDataHash()
	{
		AddDataBytesToHash(&m_params, sizeof(MeshPrimitiveParams));

		if (m_meshMaterial)
		{
			AddDataBytesToHash(m_meshMaterial->GetNameID());
		}	

		// Vertex data streams:
		for (size_t i = 0; i < m_vertexStreams.size(); i++)
		{
			if (m_vertexStreams[i])
			{
				AddDataBytesToHash(m_vertexStreams[i]->GetDataHash());
			}
		}
		if (m_indexStream)
		{
			AddDataBytesToHash(m_indexStream->GetDataHash());
		}
	}


	std::vector<re::VertexStream const*> MeshPrimitive::GetVertexStreams() const
	{
		std::vector<re::VertexStream const*> vertexStreamPtrs;
		vertexStreamPtrs.resize(Slot_Count, nullptr);

		for (uint8_t i = 0; i < Slot_Count; i++)
		{
			vertexStreamPtrs[i] = m_vertexStreams[i].get();
		}

		return vertexStreamPtrs;
	}


	char const* MeshPrimitive::SlotDebugNameToCStr(Slot slot)
	{
		switch (slot)
		{
		case Position: return ENUM_TO_STR(Position);
		case Normal: return ENUM_TO_STR(Normal);
		case Tangent: return ENUM_TO_STR(Tangent);
		case UV0: return ENUM_TO_STR(UV0);
		case Color: return ENUM_TO_STR(Color);
		case Joints: return ENUM_TO_STR(Joints);
		case Weights: return ENUM_TO_STR(Weights);
		default:
			SEAssertF("Invalid slot");
		}
		return "Invalid slot";
	}


	void MeshPrimitive::ShowImGuiWindow()
	{
		if (ImGui::CollapsingHeader(std::format("{}##{}", GetName(), GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Text(std::format("Draw mode: {}", DrawModeToCStr(m_params.m_topologyMode)).c_str());

			if (ImGui::CollapsingHeader(std::format("Material##{}", GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				m_meshMaterial->ShowImGuiWindow();
				ImGui::Unindent();
			}

			if (ImGui::CollapsingHeader(std::format("Vertex streams ({})##{}", m_vertexStreams.size(), GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				for (size_t i = 0; i < m_vertexStreams.size(); i++)
				{
					ImGui::Text(std::format("{}: {}", i, SlotToCStr(static_cast<gr::MeshPrimitive::Slot>(i))).c_str());
					if (m_vertexStreams[i])
					{
						m_vertexStreams[i]->ShowImGuiWindow();
					}
					else
					{
						ImGui::Text("<Empty>");
					}
					ImGui::Separator();
				}
				ImGui::Unindent();
			}

			if (ImGui::CollapsingHeader(std::format("Local bounds##{}", GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				m_localBounds.ShowImGuiWindow();
				ImGui::Unindent();
			}
		}
	}
	
} // re


namespace meshfactory
{
	


	
} // meshfactory


