// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Core\Interfaces\INamedObject.h"


namespace fr
{
	class EntityManager;


	class NameComponent final : public virtual en::INamedObject
	{
	public:
		static NameComponent& AttachNameComponent(EntityManager&, entt::entity, char const* name);


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		NameComponent(PrivateCTORTag, std::string const& name) : en::INamedObject(name) {}
	};
}