// © 2023 Adam Badke. All rights reserved.
#include "Transformable.h"
#include "SceneManager.h"


namespace fr
{
	Transformable::Transformable(std::string const& name, gr::Transform* parent)
		: en::NamedObject(name + "_Transformable")
		, m_transform(name, parent)
	{
		en::SceneManager::GetSceneData()->AddTransformable(this);
	}


	void Transformable::Deregister()
	{
		en::SceneManager::GetSceneData()->RemoveTransformable(this);
	}
}