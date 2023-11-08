// © 2023 Adam Badke. All rights reserved.
#include "Updateable.h"
#include "GameplayManager.h"


namespace en
{
	Updateable::Updateable()
	{
		Register();
	}


	Updateable::~Updateable()
	{
		Unregister();
	}


	void Updateable::Register()
	{
		fr::GameplayManager::Get()->AddUpdateable(this);
	}


	void Updateable::Unregister() const
	{
		fr::GameplayManager::Get()->RemoveUpdateable(this);
	}
}