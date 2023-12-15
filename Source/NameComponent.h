// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "NamedObject.h"


namespace fr
{
	class NameComponent final : public virtual en::NamedObject
	{
	public:
		NameComponent(std::string const& name) : en::NamedObject(name) {}
	};
}