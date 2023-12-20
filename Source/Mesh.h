// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Bounds.h"
#include "MeshPrimitive.h"
#include "TransformComponent.h"


namespace re
{
	class ParameterBlock;
}


namespace fr
{
	class Mesh
	{
	public:
		struct MeshConceptMarker {};

	public:
		static entt::entity CreateMeshConcept(entt::entity sceneNode, char const* name);
	};
}


namespace gr
{	
	class Transform;

	
	class Mesh
	{
	public:
		struct InstancedMeshParams
		{
			glm::mat4 g_model;
			glm::mat4 g_transposeInvModel; // For constructing the normal map TBN matrix
			static constexpr char const* const s_shaderName = "InstancedMeshParams"; // Not counted towards size of struct
		};
		static std::shared_ptr<re::ParameterBlock> CreateInstancedMeshParamsData(gr::Transform*);
		static std::shared_ptr<re::ParameterBlock> CreateInstancedMeshParamsData(std::vector<gr::Transform*> const&);
		static std::shared_ptr<re::ParameterBlock> CreateInstancedMeshParamsData(glm::mat4 const* model, glm::mat4* transposeInvModel);


		// ECS_CONVERSION TODO: IT DOESN'T REALLY MAKE SENSE TO HAVE THIS BE A MEMBER OF gr::Mesh ANYMORE!!!!
		static std::shared_ptr<re::ParameterBlock> CreateInstancedMeshParamsData(
			fr::TransformComponent::RenderData const&);
		
		static std::shared_ptr<re::ParameterBlock> CreateInstancedMeshParamsData(
			std::vector<fr::TransformComponent::RenderData const*> const&);


		// DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!
		// -> A mesh is simply a concept now, represented by an Entity and a hierarchy of components
	public:
		explicit Mesh(std::string const& name, gr::Transform* ownerTransform);
		explicit Mesh(std::string const& name, gr::Transform* ownerTransform, std::shared_ptr<gr::MeshPrimitive>);

		Mesh(Mesh const&) = default;
		Mesh(Mesh&&) = default;
		Mesh& operator=(Mesh const&) = default;
		~Mesh() = default;	

		std::string const& GetName() const;

		inline gr::Transform* GetTransform() { return m_ownerTransform; }
		inline gr::Transform const* GetTransform() const { return m_ownerTransform; }

		// Union of BoundsConcept of all attached MeshPrimitives
		inline fr::Bounds const& GetBounds() const { return m_localBounds; }
		void UpdateBounds();

		void AddMeshPrimitive(std::shared_ptr<gr::MeshPrimitive> meshPrimitive);
		std::vector<std::shared_ptr<gr::MeshPrimitive>> const& GetMeshPrimitives() const;
		void ReplaceMeshPrimitive(size_t index, std::shared_ptr<gr::MeshPrimitive> replacement);

		void ShowImGuiWindow();


	private:
		// DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!
		const std::string m_name;

		// DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!
		std::vector<std::shared_ptr<gr::MeshPrimitive>> m_meshPrimitives;

		// DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!
		Transform* m_ownerTransform; 

		// DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!
		fr::Bounds m_localBounds; // Mesh bounds, in local space

	private:
		Mesh() = delete;
	};


	inline std::string const& Mesh::GetName() const
	{
		return m_name;
	}
}