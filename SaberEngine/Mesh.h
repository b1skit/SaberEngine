#pragma once

#include <vector>
#include <memory>

#include "MeshPrimitive.h"

namespace gr
{	
	// Contains a set of mesh primitives
	class Mesh
	{
	public:
		explicit Mesh(gr::Transform* parent, std::shared_ptr<re::MeshPrimitive> meshPrimitive);

		Mesh(Mesh const&);
		Mesh(Mesh&&) = default;
		Mesh& operator=(Mesh const&) = default;
		~Mesh() = default;	

		// Getters/Setters:
		inline gr::Transform& GetTransform() { return m_transform; }
		inline gr::Transform const& GetTransform() const { return m_transform; }

		void AddMeshPrimitive(std::shared_ptr<re::MeshPrimitive> meshPrimitive);
		inline std::vector<std::shared_ptr<re::MeshPrimitive>> const& GetMeshPrimitives() const { return m_meshPrimitives; }


	private:
		std::vector<std::shared_ptr<re::MeshPrimitive>> m_meshPrimitives;
		gr::Transform m_transform;


	private:
		Mesh() = delete;
	};
}