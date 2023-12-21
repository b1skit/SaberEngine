// © 2023 Adam Badke. All rights reserved.
#include "Transformable.h"
#include "GameplayManager.h"


namespace fr
{
	Transformable::Transformable(std::string const& name, fr::Transform* parent)
		: en::NamedObject(name + "_Transformable")
		, m_transform(parent)
	{
	}


	Transformable::~Transformable()
	{
	}
}