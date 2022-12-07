#pragma once

#include "Transformable.h"
#include "Mesh.h"
#include "NamedObject.h"
#include "Updateable.h"



namespace fr
{
	class SceneObject : public virtual fr::Transformable, public virtual en::Updateable
	{
	public:
		SceneObject(gr::Transform* parent);

		SceneObject(SceneObject const& sceneObject) = default;
		SceneObject(SceneObject&&) = default;
		SceneObject& operator=(SceneObject const&) = default;
		SceneObject& operator=(SceneObject&&) = default;
		~SceneObject() = default;

		void Update(const double stepTimeMs) override {}


	private:
		SceneObject() = delete;
	};
}


