// � 2023 Adam Badke. All rights reserved.
#pragma once
#include "NamedObject.h"


namespace fr
{
	class GameplayManager;


	class NameComponent final : public virtual en::NamedObject
	{
	public:
		static NameComponent& AttachNameComponent(GameplayManager&, entt::entity, char const* name);


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		NameComponent(PrivateCTORTag, std::string const& name) : en::NamedObject(name) {}
	};
}