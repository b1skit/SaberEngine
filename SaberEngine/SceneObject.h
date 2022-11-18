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

		void Update(const double stepTimeMs) override {}

		// Getters/Setters:
		void AddMesh(std::shared_ptr<gr::Mesh> mesh);

		inline std::shared_ptr<gr::Mesh const> const GetMesh() const { return m_mesh; }
		inline std::shared_ptr<gr::Mesh> GetMesh() { return m_mesh; }

	private:
		std::shared_ptr<gr::Mesh> m_mesh;


	private:
		SceneObject() = delete;
	};
}


