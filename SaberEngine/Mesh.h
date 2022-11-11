#pragma once

#include <vector>
#include <memory>

#include "MeshPrimitive.h"
#include "Transform.h"


namespace gr
{	
	class Mesh
	{
	public:
		explicit Mesh(gr::Transform* ownerTransform, std::shared_ptr<re::MeshPrimitive> meshPrimitive);

		Mesh(Mesh const&) = default;
		Mesh(Mesh&&) = default;
		Mesh& operator=(Mesh const&) = default;
		~Mesh() = default;	

		// Getters/Setters:
		inline gr::Transform* GetTransform() { return m_ownerTransform; }
		inline gr::Transform const* GetTransform() const { return m_ownerTransform; }

		void AddMeshPrimitive(std::shared_ptr<re::MeshPrimitive> meshPrimitive);
		inline std::vector<std::shared_ptr<re::MeshPrimitive>> const& GetMeshPrimitives() const { return m_meshPrimitives; }


	private:
		std::vector<std::shared_ptr<re::MeshPrimitive>> m_meshPrimitives;

		Transform* m_ownerTransform;

	private:
		Mesh() = delete;
	};
}