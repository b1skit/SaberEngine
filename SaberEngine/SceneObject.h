#pragma once

#include "Transformable.h"
#include "Mesh.h"
#include "NamedObject.h"
#include "Updateable.h"


namespace fr
{
	class SceneObject : public virtual en::NamedObject, public virtual fr::Transformable, public virtual en::Updateable
	{
	public:
		SceneObject(std::string const& name, gr::Transform* parent);
		SceneObject(SceneObject const& sceneObject);

		SceneObject(SceneObject&&) = default;
		~SceneObject() = default;

		// NamedObject interface:
		void Update() override {}

		// Getters/Setters:
		void AddMeshPrimitive(std::shared_ptr<re::MeshPrimitive> meshPrimitive);
		inline std::vector<std::shared_ptr<gr::Mesh>> const& GetMeshes() const { return m_meshes; }


	private:
		std::vector<std::shared_ptr<gr::Mesh>> m_meshes;


	private:
		SceneObject() = delete;
	};
}


