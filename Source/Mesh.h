// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Bounds.h"
#include "MeshPrimitive.h"


namespace re
{
	class ParameterBlock;
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

		// Union of Bounds of all attached MeshPrimitives
		inline gr::Bounds& GetBounds() { return m_localBounds; }
		inline gr::Bounds const& GetBounds() const { return m_localBounds; }
		void UpdateBounds();

		void AddMeshPrimitive(std::shared_ptr<gr::MeshPrimitive> meshPrimitive);
		std::vector<std::shared_ptr<gr::MeshPrimitive>> const& GetMeshPrimitives() const;
		void ReplaceMeshPrimitive(size_t index, std::shared_ptr<gr::MeshPrimitive> replacement);

		void ShowImGuiWindow();


	private:
		const std::string m_name;

		std::vector<std::shared_ptr<gr::MeshPrimitive>> m_meshPrimitives;

		Transform* m_ownerTransform;

		gr::Bounds m_localBounds; // Mesh bounds, in local space

	private:
		Mesh() = delete;
	};


	inline std::string const& Mesh::GetName() const
	{
		return m_name;
	}
}