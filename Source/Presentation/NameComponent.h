// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IUniqueID.h"


namespace fr
{
	class EntityManager;


	class NameComponent final : public virtual core::INamedObject, public virtual core::IUniqueID
	{
	public:
		static NameComponent& AttachNameComponent(EntityManager&, entt::entity, char const* name);


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		NameComponent(PrivateCTORTag, std::string const& name) : core::INamedObject(name) {}
	};
}