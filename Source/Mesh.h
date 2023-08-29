// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Bounds.h"
#include "MeshPrimitive.h"
#include "NamedObject.h"
#include "Transform.h"

namespace re
{
	class ParameterBlock;
}

namespace gr
{	
	class Mesh final : public virtual en::NamedObject
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


	public:
		explicit Mesh(std::string const& name, gr::Transform* ownerTransform);
		explicit Mesh(std::string const& name, gr::Transform* ownerTransform, std::shared_ptr<re::MeshPrimitive> meshPrimitive);

		Mesh(Mesh const&) = default;
		Mesh(Mesh&&) = default;
		Mesh& operator=(Mesh const&) = default;
		~Mesh() = default;	

		// Getters/Setters:
		inline gr::Transform* GetTransform() { return m_ownerTransform; }
		inline gr::Transform const* GetTransform() const { return m_ownerTransform; }

		// Union of Bounds of all attached MeshPrimitives
		inline gr::Bounds& GetBounds() { return m_localBounds; }
		inline gr::Bounds const& GetBounds() const { return m_localBounds; }
		void UpdateBounds();

		void AddMeshPrimitive(std::shared_ptr<re::MeshPrimitive> meshPrimitive);
		std::vector<std::shared_ptr<re::MeshPrimitive>> const& GetMeshPrimitives() const;
		void ReplaceMeshPrimitive(size_t index, std::shared_ptr<re::MeshPrimitive> replacement);

		void ShowImGuiWindow();


	private:
		std::vector<std::shared_ptr<re::MeshPrimitive>> m_meshPrimitives;

		Transform* m_ownerTransform;

		gr::Bounds m_localBounds; // Mesh bounds, in local space	

	private:
		Mesh() = delete;
	};
}