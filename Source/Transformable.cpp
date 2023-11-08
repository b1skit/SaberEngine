// © 2023 Adam Badke. All rights reserved.
#include "Transformable.h"
#include "GameplayManager.h"


namespace fr
{
	Transformable::Transformable(std::string const& name, gr::Transform* parent)
		: en::NamedObject(name + "_Transformable")
		, m_transform(name, parent)
	{
		Register();
	}


	Transformable::~Transformable()
	{
		Deregister();
	}


	void Transformable::Register()
	{
		fr::GameplayManager::Get()->AddTransformable(this);
	}


	void Transformable::Deregister() const
	{
		fr::GameplayManager::Get()->RemoveTransformable(this);
	}
}